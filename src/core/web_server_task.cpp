#include "core/web_server_task.h"
#include "core/log_task.h"
#include "core/wifi_task.h"
#include "core/core_engine.h"
#include "core/info.h"
#include "config.h"
#include "html/main_html.h"
#include "html/info_html.h"
#include "html/tools_html.h"
#include "html/console_html.h"
#include "html/ota_html.h"
#include "html/config_html.h"
#include "html/config_module_html.h"
#include "html/manage_file_system_html.h"
#include "core/pin_config.h"
#include "core/dispatcher.h"
#include <Arduino.h>
#include <WiFi.h>
#include <semphr.h>
#include <ArduinoJson.h>

#define RESTART_DELAY_MS 2000
#define OTA_RESTART_DELAY_MS 500

/* -------------------------------------------------------------------------- */
/*                              STATIC VARIABLES                              */
/* -------------------------------------------------------------------------- */

static uint16_t web_port      = 0;
static String web_username    = "";
static String web_password    = "";
static AsyncWebServer *server = NULL;
static SemaphoreHandle_t webConfigMutex = NULL;
/* For register rows in table*/
static String tableRowsHTML = "";
/* Buffer dynamic JSON for HTTP Polling responses */
static JsonDocument telemetryDoc;
static String telemetryJson = "{}";

/* -------------------------------------------------------------------------- */
/*                              GLOBAL VARIABLES                              */
/* -------------------------------------------------------------------------- */

AsyncWebSocket ws("/ws");

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief Initializes web server mutex. */
static void initWebMutex() {
    if (webConfigMutex == NULL) {
        webConfigMutex = xSemaphoreCreateMutex();
    }
}

/** @brief Formats seconds into uptime string format (e.g. "0T00:01:23"). */
static String formatUptime(uint32_t seconds) {
    uint32_t days = seconds / 86400;
    seconds %= 86400;
    uint32_t hrs = seconds / 3600;
    seconds %= 3600;
    uint32_t mins = seconds / 60;
    uint32_t secs = seconds % 60;
    char buf[32];
    snprintf(buf, sizeof(buf), "%uT%02u:%02u:%02u", days, hrs, mins, secs);
    return String(buf);
}

/** @brief Generates dynamic GPIO selector rows from BOARD_PINS and AVAILABLE_PIN_OPTIONS. */
static String generatePinRows() {
    String rows = "";
    for (size_t i = 0; i < BOARD_PIN_COUNT; i++) {
        const auto& pin = BOARD_PINS[i];
        String activeName = get_pin_name(pin.gpio);
        if (activeName.length() == 0) {
            activeName = pin.defaultOption;
        }
        
        if (pin.isFixed) {
            rows += "<div class='form-row'>";
            rows += "<label for='gpio" + String(pin.gpio) + "' class='gpio-red'>" + pin.label + "</label>";
            rows += "<select id='gpio" + String(pin.gpio) + "' name='gpio" + String(pin.gpio) + "' disabled>";
            rows += "<option value='-1' selected>" + String(pin.defaultOption) + "</option>";
            rows += "</select></div>";
        } else {
            rows += "<div class='form-row'>";
            rows += "<label for='gpio" + String(pin.gpio) + "'>" + pin.label + "</label>";
            rows += "<select id='gpio" + String(pin.gpio) + "' name='gpio" + String(pin.gpio) + "'>";
            
            for (size_t j = 0; j < AVAILABLE_PIN_OPTIONS_COUNT; j++) {
                const char* optName = AVAILABLE_PIN_OPTIONS[j];
                bool isSelected = activeName.equalsIgnoreCase(optName);
                String selectedAttr = isSelected ? " selected" : "";
                
                rows += "<option value='" + String(optName) + "'" + selectedAttr + ">" + String(optName) + "</option>";
            }

            rows += "</select></div>";
        }
    }
    return rows;
}

/** @brief Renders HTML templates by replacing placeholders. */
static String renderTemplate(const char* templateStr) {
    String page = String(templateStr);
    page.replace("%HEADER_TITLE%", "ESP32S3");
    page.replace("%HEADER_SUBTITLE%", "ESP mini framework " + String(FIRMWARE_VERSION));
    page.replace("%FOOTER_TEXT%", "ESP mini framework " + String(FIRMWARE_VERSION) + " by Văn Tiến");
    page.replace("%APP_VERSION%", String(FIRMWARE_VERSION));
    page.replace("%BUILD_DATE%", String(__DATE__) + " " + String(__TIME__));
    page.replace("%SDK_VERSION%", String(ESP.getSdkVersion()));
    page.replace("%HOSTNAME%", "tasmota-" + String(esp_info_get_mac_str()));
    page.replace("%CHIP_MODEL%", String(esp_info_get_model()));
    page.replace("%MAC_ADDR%", String(esp_info_get_mac_str()));
    page.replace("%FLASH_SIZE%", String(ESP.getFlashChipSize() / 1024) + " KB");
    page.replace("%SENSOR_TABLE_ROWS%", tableRowsHTML);
    page.replace("%GPIO_TABLE_ROWS%", generatePinRows());
    page.replace("%UPTIME%", formatUptime(millis() / 1000));
    page.replace("%IP_ADDR%", getWifiSSID());
    page.replace("%GATEWAY%", WiFi.gatewayIP().toString());
    page.replace("%SUBNET_MASK%", WiFi.subnetMask().toString());
    page.replace("%DNS_SERVER%", WiFi.dnsIP().toString());
    page.replace("%FREE_RAM%", String(esp_info_get_free_heap() / 1024.0, 1) + " KB");
    return page;
}

/** @brief Clears the telemetry JSON data. */
static void clearTelemetryJson() {
    telemetryJson = "{}";
    telemetryDoc.clear();
}

/**
 * @brief Saves current in-memory web server configuration parameters to NVS.
 */
static void saveWebConfig() {
    if (webConfigMutex != NULL && xSemaphoreTake(webConfigMutex, portMAX_DELAY) == pdTRUE) {
        if (config_get_lock()) {
            config_save_int("web", "port", web_port);
            config_save_string("web", "username", web_username);
            config_save_string("web", "password", web_password);
            config_release_lock();
        }
        xSemaphoreGive(webConfigMutex);
    }
}

/**
 * @brief Loads web server configuration from NVS.
 */
static void loadWebConfig() {
    initWebMutex();
    register_config_module("web", "web");

    if (webConfigMutex != NULL && xSemaphoreTake(webConfigMutex, portMAX_DELAY) == pdTRUE) {
        if (config_get_lock()) {
            web_port = (uint16_t)config_read_int("web", "port", WEB_PORT);
            web_username = config_read_string("web", "username", WEB_USERNAME);
            web_password = config_read_string("web", "password", WEB_PASSWORD);
            config_release_lock();
        }
        xSemaphoreGive(webConfigMutex);
    }
    LOG_INFO("web config loaded from NVS successfully.");
}

/* ---------------- web server call back----------------- */

/**
 * @brief Handles HTTP GET request for the root dashboard URL ("/").
 * Authenticates user credentials and sends the rendered main dashboard HTML.
 */
static void handleRoot(AsyncWebServerRequest *request) {
    if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
        return request->requestAuthentication();
    }
    request->send(200, "text/html", renderTemplate(MAIN_HTML));
}

/**
 * @brief Handles HTTP GET request for the system information page ("/info").
 * Authenticates user and responds with the rendered system info HTML page.
 */
static void handleInfo(AsyncWebServerRequest *request) {
    if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
        return request->requestAuthentication();
    }
    request->send(200, "text/html", renderTemplate(INFO_HTML));
}

/**
 * @brief Handles HTTP GET request for the tools page ("/tools").
 * Authenticates user and serves the tools overview HTML interface.
 */
static void handleTools(AsyncWebServerRequest *request) {
    if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
        return request->requestAuthentication();
    }
    request->send(200, "text/html", renderTemplate(TOOLS_HTML));
}

/**
 * @brief Handles HTTP GET request for the LittleFS file system manager ("/tools/manage_file_system").
 * Authenticates user and serves the file system management HTML page.
 */
static void handleManageFileSystem(AsyncWebServerRequest *request) {
    if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
        return request->requestAuthentication();
    }
    request->send(200, "text/html", renderTemplate(MANAGE_FILE_SYSTEM_HTML));
}

/**
 * @brief Handles HTTP GET request for the web console terminal ("/console").
 * Authenticates user and serves the interactive serial/web console HTML interface.
 */
static void handleConsole(AsyncWebServerRequest *request) {
    if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
        return request->requestAuthentication();
    }
    request->send(200, "text/html", renderTemplate(CONSOLE_HTML));
}

/**
 * @brief Handles HTTP GET request for the firmware upgrade page ("/ota").
 * Authenticates user and serves the OTA firmware upload HTML page.
 */
static void handleOta(AsyncWebServerRequest *request) {
    if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
        return request->requestAuthentication();
    }
    request->send(200, "text/html", renderTemplate(OTA_HTML));
}

/**
 * @brief Handles HTTP GET request for the general configuration page ("/config").
 * Authenticates user and serves the WiFi/network settings HTML page.
 */
static void handleConfig(AsyncWebServerRequest *request) {
    if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
        return request->requestAuthentication();
    }
    request->send(200, "text/html", renderTemplate(CONFIG_HTML));
}

/**
 * @brief Handles HTTP GET request for the GPIO pin mapping configuration page ("/config-module").
 * Authenticates user and serves the module pin configuration HTML page.
 */
static void handleConfigModule(AsyncWebServerRequest *request) {
    if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
        return request->requestAuthentication();
    }
    request->send(200, "text/html", renderTemplate(CONFIG_MODULE_HTML));
}

/**
 * @brief Fallback handler for unmapped routes and 404 errors.
 * Logs the unhandled URL to Serial and responds with HTTP 404 Not Found.
 */
static void handleNotFound(AsyncWebServerRequest *request) {
    Serial.printf("[Web Error] Not Found / Internal error on URL: %s\n", request->url().c_str());
    request->send(404, "text/plain", "Not found");
}

/**
 * @brief Handles HTTP GET request for retrieving current WiFi credentials ("/getConfig").
 * Authenticates user and responds with JSON containing current WiFi SSID and password.
 */
static void handleGetConfig(AsyncWebServerRequest *request) {
    if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
        return request->requestAuthentication();
    }

    String json = "{";
    json += "\"wifiSSID\":\"" + getWifiSSID() + "\",";
    json += "\"wifiPass\":\"" + getWifiPassword() + "\"";
    json += "}";
    request->send(200, "application/json", json);
}

/**
 * @brief Handles HTTP POST request headers for saving WiFi credentials ("/saveConfig").
 * Placeholder callback required by AsyncWebServer before handling body payload.
 */
static void handleSaveConfigRequest(AsyncWebServerRequest *request) {
    // Request processing is deferred to handleSaveConfigBody once body payload is received
}

/**
 * @brief Handles HTTP POST body data containing new WiFi credentials in JSON format ("/saveConfig").
 * Parses SSID and password, replies OK, and spawns a background task to update config and restart.
 */
static void handleSaveConfigBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
        return request->requestAuthentication();
    }

    String json = "";
    for (size_t i = 0; i < len; i++) {
        json += (char)data[i];
    }

    int idx1 = json.indexOf("\"wifiSSID\":\"");
    int idx2 = json.indexOf("\"wifiPass\":\"");
    if (idx1 != -1 && idx2 != -1) {
        idx1 += 12;
        String newSsid = json.substring(idx1, json.indexOf("\"", idx1));
        idx2 += 12;
        String newPass = json.substring(idx2, json.indexOf("\"", idx2));

        request->send(200, "text/plain", "OK");

        struct WifiSaveArgs { String ssid; String pass; };
        WifiSaveArgs *args = new WifiSaveArgs{newSsid, newPass};

        // Create background task to save WiFi credentials and trigger deferred system restart
        xTaskCreate([](void *arg) {
            WifiSaveArgs *a = (WifiSaveArgs*)arg;
            updateWifiConfig(a->ssid, a->pass);
            delete a;
            vTaskDelay(pdMS_TO_TICKS(RESTART_DELAY_MS));
            postIncomingCommand(CMD_RESTART);
            vTaskDelete(NULL);
        }, "save_wifi_task", 4096, args, 1, NULL);
    } else {
        request->send(400, "text/plain", "Invalid JSON Payload");
    }
}

/**
 * @brief Handles HTTP GET request for command execution ("/cmd").
 * Extracts the 'msg' query parameter and posts the incoming command into system queue.
 */
static void handleCmd(AsyncWebServerRequest *request) {
    if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
        return request->requestAuthentication();
    }

    if (request->hasParam("msg")) {
        String cmd = request->getParam("msg")->value();
        cmd.trim();
        if (cmd.length() > 0) {
            postIncomingCommand(cmd);
        }

        request->send(200, "text/plain", "Command queued: " + cmd);
    } else {
        request->send(400, "text/plain", "Missing msg parameter");
    }
}

/**
 * @brief Handles HTTP POST request to reboot the device ("/restart").
 * Sends an acknowledgment response and creates a FreeRTOS task to trigger system restart.
 */
static void handleRestart(AsyncWebServerRequest *request) {
    if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
        return request->requestAuthentication();
    }

    request->send(200, "text/plain", "Restarting device...");
    xTaskCreate([](void *arg) {
        vTaskDelay(pdMS_TO_TICKS(RESTART_DELAY_MS));
        postIncomingCommand(CMD_RESTART);
        vTaskDelete(NULL); // Delete task itself
    }, "deferred_restart", 2048, NULL, 1, NULL);
}

/**
 * @brief Handles HTTP POST request to save module GPIO pin configuration ("/saveModule").
 * Extracts pin assignments from request parameters, saves to flash, and restarts device.
 */
static void handleSaveModule(AsyncWebServerRequest *request) {
    if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
        return request->requestAuthentication();
    }

    for (int i = 0; i < MAX_GPIO_PINS; i++) {
        String paramName = "gpio" + String(i);
        // Check if the parameter exists in the request and update the pin name accordingly
        if (request->hasParam(paramName, true)) {
            set_pin_name(i, request->getParam(paramName, true)->value());
        }
    }
    request->send(200, "text/plain", "OK");
    // Spawn a background task to save config to Flash and initiate system restart non-blockingly
    xTaskCreate([](void *arg) {
        // Save GPIO configuration to non-volatile storage
        pin_config_save();
        // Delay execution briefly to ensure HTTP response transmission completes
        vTaskDelay(pdMS_TO_TICKS(RESTART_DELAY_MS));
        // Post restart command to system queue
        postIncomingCommand(CMD_RESTART);
        // Delete current task to release allocated stack memory
        vTaskDelete(NULL);
    }, "save_restart_task", 4096, NULL, 1, NULL);
}

/**
 * @brief Handles HTTP GET request for sensor telemetry data ("/api/telemetry").
 * Triggers telemetry signal, responds with buffered telemetry JSON, and clears buffer.
 */
static void handleTelemetry(AsyncWebServerRequest *request) {
    if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
        return request->requestAuthentication();
    }
    // Trigger telemetry data update before sending
    dispatch_signal(SIG_WEB_POLL); 
    request->send(200, "application/json", telemetryJson);
    // Clear telemetryJson after sending to prepare for next update
    clearTelemetryJson();
}

/**
 * @brief Handles the final response for OTA firmware update ("/doUpdate").
 * Verifies upload result, returns HTTP 200 with OK/FAIL, and schedules reboot on success.
 */
static void handleOtaUpdateRequest(AsyncWebServerRequest *request) {
    // Step 1: Verify HTTP Basic Authentication credentials.
    // Returns 401 Unauthorized headers if authentication fails.
    if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
        return request->requestAuthentication();
    }

    // Step 2: Check if any Flash write/verification errors occurred during data stream processing.
    bool success = !Update.hasError();

    // Step 3: Build plain-text HTTP response payload ("OK" or "FAIL").
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", success ? "OK" : "FAIL");
    
    // Instruct client to close TCP connection immediately after receiving response.
    response->addHeader("Connection", "close");
    
    // Transmit HTTP response back to client asynchronously.
    request->send(response);

    // Step 4: If update succeeded, schedule a non-blocking system restart via FreeRTOS Task.
    if (success) {
        xTaskCreate([](void *arg) {
            // Delay execution to allow AsyncWebServer enough time to flush HTTP response buffer to client.
            vTaskDelay(pdMS_TO_TICKS(OTA_RESTART_DELAY_MS));
            
            // Enqueue system restart command to main system task.
            postIncomingCommand(CMD_RESTART);
            
            // Self-terminate background task to prevent memory leak.
            vTaskDelete(NULL);
        }, "ota_restart_task", 2048, NULL, 1, NULL);
    }
}

/**
 * @brief Handles streaming chunks of binary firmware data during OTA update ("/doUpdate").
 * Initializes OTA partition, writes incoming data chunks to Flash, and verifies on completion.
 */
static void handleOtaUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    // Step 1: Secure raw binary data callback stream against unauthorized uploads.
    if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
        return;
    }

    // Step 2: INITIALIZATION (Executes only for the first incoming data packet when index == 0).
    if (!index) {
        LOG_INFO("OTA Update started, filename: " + filename);

        // Update.begin():
        // 1. Identifies the inactive partition (e.g., ota_1) from partition table.
        // 2. Erases target Flash partition range to prepare for writing incoming image.
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            LOG_ERROR("OTA Begin Error!");
            Update.printError(Serial);
        }
    }

    // Step 3: FLASH WRITE (Executes for every incoming data packet chunk).
    // Update.write(): Writes raw byte buffer directly into inactive Flash partition via SPI bus.
    if (Update.write(data, len) != len) {
        LOG_ERROR("OTA Write Error!");
        Update.printError(Serial);
    }

    // Step 4: FINALIZATION (Executes only for the last packet of the file upload).
    if (final) {
        // Update.end(true):
        // 1. Verifies binary checksum/MD5 and image headers.
        // 2. Writes new boot target marker into 'otadata' partition, setting inactive partition as active for next reboot.
        if (Update.end(true)) {
            LOG_INFO("OTA Update Success! Written bytes: " + String(index + len));
        } else {
            LOG_ERROR("OTA End Error!");
            Update.printError(Serial);
        }
    }
}

/**
 * @brief WebSocket event handler for "/ws" terminal console endpoint.
 * Handles client connection, disconnection, and incoming text commands.
 */
static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
               void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WebSocket client #%u connected\n", client->id());
        client->text("=== Sensor Monitor Connected ===");
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            String msgStr = "";
            for (size_t i = 0; i < len; i++) {
                msgStr += (char)data[i];
            }
            msgStr.trim();
            if (msgStr.length() > 0) {
                postIncomingCommand(msgStr);
            }
        }
    }
}

/**
 * @brief Configures AsyncWebServer endpoints, WebSocket handlers, and authentication.
 */
static void setupWebServer() {
    uint16_t port = getWebPort();
    if (server != NULL) {
        LOG_INFO("Cleaning up old server instance...");
        delete server;
        server = NULL;
    }
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");
    server = new AsyncWebServer(port);

    // WebSocket Setup
    ws.setAuthentication(getWebUsername().c_str(), getWebPassword().c_str());
    ws.onEvent(onWsEvent);
    server->addHandler(&ws);

    // Page Routes
    server->on("/", HTTP_GET, handleRoot);
    server->on("/info", HTTP_GET, handleInfo);
    server->on("/tools", HTTP_GET, handleTools);
    server->on("/tools/manage_file_system", HTTP_GET, handleManageFileSystem);
    server->on("/console", HTTP_GET, handleConsole);
    server->on("/ota", HTTP_GET, handleOta);
    server->on("/config", HTTP_GET, handleConfig);
    server->on("/config-module", HTTP_GET, handleConfigModule);

    // Fallback 404 Route
    server->onNotFound(handleNotFound);

    // REST API & Action Endpoints
    server->on("/getConfig", HTTP_GET, handleGetConfig);
    server->on("/saveConfig", HTTP_POST, handleSaveConfigRequest, NULL, handleSaveConfigBody);
    server->on("/cmd", HTTP_GET, handleCmd);
    server->on("/restart", HTTP_POST, handleRestart);
    server->on("/saveModule", HTTP_POST, handleSaveModule);
    server->on("/api/telemetry", HTTP_GET, handleTelemetry);

    // OTA Firmware Upload Endpoint
    server->on("/doUpdate", HTTP_POST, handleOtaUpdateRequest, handleOtaUpload);

    server->begin();
    setWebLogReady();
    Serial.println("Web Server started on port " + String(getWebPort()));
    Serial.println("Open: http://" + WiFi.localIP().toString() + ":" + String(getWebPort()));
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */
uint8_t registerElement(const String& label, const String& unit, const String& initialValue) {
    updateElementValue(label, initialValue);
    LOG_DEBUG("tableRowsHTML: " + tableRowsHTML);
    return 0;
}

void updateElementValue(uint8_t id, const String& newValue) {
    updateElementValue(String(id), newValue);
}

void updateElementValue(const String& key, const String& newValue) {
    // Update the value in the telemetry JSON document
    telemetryDoc[key] = newValue;

    // Serialize JsonDocument to telemetryJson string for HTTP Polling responses
    telemetryJson = "";
    serializeJson(telemetryDoc, telemetryJson);
}

uint16_t getWebPort() { 
    return web_port;
}

String getWebUsername() {
    return web_username;
}

String getWebPassword() {
    return web_password;
}

void updateWebConfig(uint16_t port, const String &user, const String &pass) {
    if (webConfigMutex != NULL && xSemaphoreTake(webConfigMutex, portMAX_DELAY) == pdTRUE) {
        web_port = port;
        web_username = user;
        web_password = pass;
        xSemaphoreGive(webConfigMutex);
    }
    saveWebConfig();
}

/**
 * @brief Web Server & System Maintenance Task
 * @priority 1 (Low Priority)
 * @core Core 1
 * 
 * Periodically cleans up inactive WebSocket clients and performs background maintenance.
 */
void vWebMonitorTask(void *pvParameters) {
    waiting_on_event(NETWORK_EVENT, NET_STATE_WIFI_STA, portMAX_DELAY);
    loadWebConfig();	
    setupWebServer();
    for (;;) {
        ws.cleanupClients();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
