#include "core/web_server_task.h"
#include "core/log_task.h"
#include "core/wifi_task.h"
#include "core/core_engine.h"
#include "core/chip_info.h"
#include "config.h"
#include "html/main_html.h"
#include "html/info_html.h"
#include "html/tools_html.h"
#include "html/console_html.h"
#include "html/ota_html.h"

#include <Arduino.h>
#include <WiFi.h>
#include <semphr.h>

static uint16_t web_port      = 0;
static String web_username    = "";
static String web_password    = "";

static AsyncWebServer *server= NULL;
AsyncWebSocket ws("/ws");

static SemaphoreHandle_t webConfigMutex = NULL;

/**
 * @brief Initializes the FreeRTOS mutex for protecting Web server->configuration.
 * @param None
 * @return None
 */
static void initWebMutex() {
    if (webConfigMutex == NULL) {
        webConfigMutex = xSemaphoreCreateMutex();
    }
}

void loadWebConfig() {
    register_config_file("web", "web_config.txt");
    initWebMutex();

    String raw = read_config("web");
    if (raw.length() == 0) {
        LOG_INFO("web config file not found or empty. creating default web_config.txt");
		web_username = WEB_USERNAME;
		web_password = WEB_PASSWORD;
		web_port = WEB_PORT;
        saveWebConfig();
        return;
    }

    int pos = 0;
    while (pos < raw.length()) {
        int nextpos = raw.indexOf('\n', pos);
        if (nextpos == -1) nextpos = raw.length();
        String line = raw.substring(pos, nextpos);
        line.trim();
        pos = nextpos + 1;

        if (line.length() == 0) continue;
        int eqidx = line.indexOf('=');
        if (eqidx > 0) {
            String key = line.substring(0, eqidx);
            String val = line.substring(eqidx + 1);
            key.trim();
            val.trim();
            if (key.equalsIgnoreCase("port")) {
                web_port = (uint16_t)val.toInt();
            } else if (key.equalsIgnoreCase("username")) {
                web_username = val;
            } else if (key.equalsIgnoreCase("password")) {
                web_password = val;
            }
        }
    }
    LOG_INFO("web config loaded successfully.");
}

void saveWebConfig() {
    String content = "";
    if (webConfigMutex != NULL && xSemaphoreTake(webConfigMutex, portMAX_DELAY) == pdTRUE) {
        content += "port=" + String(web_port) + "\n";
        content += "username=" + web_username + "\n";
        content += "password=" + web_password + "\n";
        xSemaphoreGive(webConfigMutex);
    }
    save_config("web", content);
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

// Web Server & WebSocket Instances

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
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
                postIncomingCommand(msgStr, CMD_SOURCE_WEB);
            }
        }
    }
}

void setupWebServer() {
	uint16_t port = getWebPort();
	if (server != NULL) {
		LOG_INFO("Cleaning up old server instance...");
		delete server;
		server = NULL;
	}
    server = new AsyncWebServer(port);
    // WebSocket Setup
    ws.setAuthentication(getWebUsername().c_str(), getWebPassword().c_str());
    ws.onEvent(onWsEvent);
    server->addHandler(&ws);

    // Page 1: Main Menu UI
    server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
            return request->requestAuthentication();
        }
        request->send(200, "text/html", MAIN_HTML);
    });

    // Page 2: Information UI
    server->on("/info", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
            return request->requestAuthentication();
        }
        String html = String(INFO_HTML);
        html.replace("%LOCAL_IP%", WiFi.localIP().toString());
        html.replace("%WEB_USERNAME%", getWebUsername());
        html.replace("%WEB_PASSWORD%", getWebPassword());
        request->send(200, "text/html", html);
    });

    // Page 3: Tools UI
    server->on("/tools", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
            return request->requestAuthentication();
        }
        request->send(200, "text/html", TOOLS_HTML);
    });

    // Page 4: Console UI
    server->on("/console", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
            return request->requestAuthentication();
        }
        String html = String(CONSOLE_HTML);
        html.replace("%WEB_USERNAME%", getWebUsername());
        html.replace("%WEB_PASSWORD%", getWebPassword());
        request->send(200, "text/html", html);
    });

    // Page 5: Firmware Upgrade UI
    server->on("/ota", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
            return request->requestAuthentication();
        }
        String html = String(OTA_HTML);
        html.replace("%WEB_USERNAME%", getWebUsername());
        html.replace("%WEB_PASSWORD%", getWebPassword());
        request->send(200, "text/html", html);
    });


    // Endpoint: Device Telemetry & Statistics
    server->on("/stats", HTTP_GET, [](AsyncWebServerRequest *request) {
        // if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
        //     return request->requestAuthentication();
        // }

        String json = "{";
        json += "\"uptime\":" + String(millis() / 1000) + ",";
        json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
        json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
        json += "\"mac\":\"" + WiFi.macAddress() + "\",";
        json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"gw\":\"" + WiFi.gatewayIP().toString() + "\",";
        json += "\"mask\":\"" + WiFi.subnetMask().toString() + "\",";
        json += "\"dns1\":\"" + WiFi.dnsIP().toString() + "\",";
        json += "\"dns2\":\"0.0.0.0\",";
        json += "\"programVersion\":\"15.5.0.1 (tasmota32)\",";
        json += "\"buildDate\":\"" + String(__DATE__) + "T" + String(__TIME__) + "\",";
        json += "\"sdkVersion\":\"" + String(ESP.getSdkVersion()) + "\",";
        json += "\"hostname\":\"tasmota-" + WiFi.macAddress() + "\",";
        json += "\"chipModel\":\"" + String(ESP.getChipModel()) + "\",";
        json += "\"flashSize\":\"" + String(ESP.getFlashChipSize() / 1024) + " KB\",";
        json += "\"headerTitle\":\"ESP32S3\",";
        json += "\"headerSubTitle\":\"ESP mini framework " + String(FIRMWARE_VERSION) + "\",";
        json += "\"footerText\":\"ESP mini framework " + String(FIRMWARE_VERSION) + " by Văn Tiến\"";
        json += "}";
        request->send(200, "application/json", json);
    });



    // Endpoint: Retrieve Device Configuration
    server->on("/getConfig", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
            return request->requestAuthentication();
        }

        String json = "{";
        json += "\"wifiSSID\":\"" + getWifiSSID() + "\",";
        json += "\"wifiPass\":\"" + getWifiPassword() + "\"";
        json += "}";
        request->send(200, "application/json", json);
    });

    // Endpoint: Update Device Configuration
    server->on("/saveConfig", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
                return request->requestAuthentication();
            }

            String json = "";
            for (size_t i = 0; i < len; i++) {
                json += (char)data[i];
            }

            int idx;
            idx = json.indexOf("\"wifiSSID\":\"") + 12;
            String newSsid = json.substring(idx, json.indexOf("\"", idx));

            idx = json.indexOf("\"wifiPass\":\"") + 12;
            String newPass = json.substring(idx, json.indexOf("\"", idx));

            updateWifiConfig(newSsid, newPass);
            request->send(200, "text/plain", "OK");
            vTaskDelay(pdMS_TO_TICKS(2000));
            ESP.restart();
        }
    );

    // Endpoint: Execute System Commands
    server->on("/cmd", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
            return request->requestAuthentication();
        }

        if (request->hasParam("msg")) {
            String cmd = request->getParam("msg")->value();
            cmd.trim();
            if (cmd.length() > 0) {
                postIncomingCommand(cmd, CMD_SOURCE_WEB);
            }

            request->send(200, "text/plain", "Command queued: " + cmd);
        } else {
            request->send(400, "text/plain", "Missing msg parameter");
        }
    });

    // Endpoint: Reset Configuration to Defaults
    server->on("/resetConfig", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
            return request->requestAuthentication();
        }

        // Reset wifi and web configs to defaults
        updateWifiConfig("Juniper_Secured", "vs353535");
        updateWebConfig(8088, "ittien", "Remtoiyeuemilia1@");

        request->send(200, "text/plain", "Configuration reset! Restarting with default settings...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        ESP.restart();
    });


    // Endpoint: OTA Firmware Upload Handler
    server->on("/doUpdate", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
                return request->requestAuthentication();
            }
            bool success = !Update.hasError();
            AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", success ? "OK" : "FAIL");
            response->addHeader("Connection", "close");
            request->send(response);
            vTaskDelay(pdMS_TO_TICKS(1000));
            ESP.restart();
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            if (!index) {
                Serial.printf("Update: %s\n", filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Update.printError(Serial);
                }
            }
            if (Update.write(data, len) != len) {
                Update.printError(Serial);
            }
            if (final) {
                if (Update.end(true)) {
                    Serial.printf("Update Success: %u bytes\n", index + len);
                } else {
                    Update.printError(Serial);
                }
            }
        }
    );

    server->begin();
    setWebLogReady();
    Serial.println("Web Server started on port " + String(getWebPort()));
    Serial.println("Open: http://" + WiFi.localIP().toString() + ":" + String(getWebPort()));
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
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

