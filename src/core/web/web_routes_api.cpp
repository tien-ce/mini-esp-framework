#include "core/web/web_routes_api.h"
#include "core/web/web_auth.h"
#include "core/web/web_template.h"
#include "core/wifi_task.h"
#include "core/pin_config.h"
#include "core/dispatcher.h"
#include "core/log_task.h"
#include "config.h"
#include <Arduino.h>
#include <ArduinoJson.h>

#define RESTART_DELAY_MS 2000

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief Handles HTTP GET request for retrieving current WiFi credentials ("/getConfig"). */
static void handleGetConfig(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;

    String json = "{";
    json += "\"wifiSSID\":\"" + getWifiSSID() + "\",";
    json += "\"wifiPass\":\"" + getWifiPassword() + "\"";
    json += "}";
    request->send(200, "application/json", json);
}

/** @brief Handles HTTP POST request headers for saving WiFi credentials ("/saveConfig"). */
static void handleSaveConfigRequest(AsyncWebServerRequest *request) {
    // Request processing is deferred to handleSaveConfigBody once body payload is received
}

/** @brief Handles HTTP POST body data containing new WiFi credentials in JSON format ("/saveConfig"). */
static void handleSaveConfigBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!web_authenticate(request)) return;

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

/** @brief Handles HTTP GET request for command execution ("/cmd"). */
static void handleCmd(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;

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

/** @brief Handles HTTP POST request to reboot the device ("/restart"). */
static void handleRestart(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;

    request->send(200, "text/plain", "Restarting device...");
    xTaskCreate([](void *arg) {
        vTaskDelay(pdMS_TO_TICKS(RESTART_DELAY_MS));
        postIncomingCommand(CMD_RESTART);
        vTaskDelete(NULL);
    }, "deferred_restart", 2048, NULL, 1, NULL);
}

/** @brief Handles HTTP POST request to save module GPIO pin configuration ("/saveModule"). */
static void handleSaveModule(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;

    for (int i = 0; i < MAX_GPIO_PINS; i++) {
        String paramName = "gpio" + String(i);
        if (request->hasParam(paramName, true)) {
            set_pin_name(i, request->getParam(paramName, true)->value());
        }
    }
    request->send(200, "text/plain", "OK");

    xTaskCreate([](void *arg) {
        pin_config_save();
        vTaskDelay(pdMS_TO_TICKS(RESTART_DELAY_MS));
        postIncomingCommand(CMD_RESTART);
        vTaskDelete(NULL);
    }, "save_restart_task", 4096, NULL, 1, NULL);
}

/** @brief Handles HTTP GET request for sensor telemetry data ("/api/telemetry"). */
static void handleTelemetry(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;

    dispatch_signal(SIG_WEB_POLL);
    request->send(200, "application/json", web_get_telemetry_json());
    web_clear_telemetry_json();
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

void register_api_routes(AsyncWebServer *server) {
    if (server == nullptr) return;

    server->on("/getConfig", HTTP_GET, handleGetConfig);
    server->on("/saveConfig", HTTP_POST, handleSaveConfigRequest, NULL, handleSaveConfigBody);
    server->on("/cmd", HTTP_GET, handleCmd);
    server->on("/restart", HTTP_POST, handleRestart);
    server->on("/saveModule", HTTP_POST, handleSaveModule);
    server->on("/api/telemetry", HTTP_GET, handleTelemetry);
}
