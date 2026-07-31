#include "web_server.h"
#include "freertos_tasks.h"
#include "log_task.h"
#include "wifi_task.h"
#include <esp_mac.h>
#include <Arduino.h>
#include <WiFi.h>

// Web Server & WebSocket Instances
AsyncWebServer server(WEB_PORT);
AsyncWebSocket ws("/ws");

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
    // WebSocket Setup
    ws.setAuthentication(WEB_USERNAME, WEB_PASSWORD);
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // Endpoint: Main Dashboard UI
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->authenticate(WEB_USERNAME, WEB_PASSWORD)) {
            return request->requestAuthentication();
        }

        String html = String(INDEX_HTML);
        html.replace("%LOCAL_IP%", WiFi.localIP().toString());
        html.replace("%FIRMWARE_VERSION%", FIRMWARE_VERSION);
        html.replace("%WEB_USERNAME%", WEB_USERNAME);
        html.replace("%WEB_PASSWORD%", WEB_PASSWORD);

        request->send(200, "text/html", html);
    });

    // Endpoint: Device Telemetry & Statistics
    server.on("/stats", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->authenticate(WEB_USERNAME, WEB_PASSWORD)) {
            return request->requestAuthentication();
        }

        String json = "{";
        json += "\"uptime\":" + String(millis() / 1000) + ",";
        json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
        json += "\"count\":" + String(getSensorCount()) + ",";
        json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
        json += "\"clientID\":\"" + getClientID() + "\"";
        json += "}";
        request->send(200, "application/json", json);
    });

    // Endpoint: Retrieve Device Configuration
    server.on("/getConfig", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->authenticate(WEB_USERNAME, WEB_PASSWORD)) {
            return request->requestAuthentication();
        }

        String json = "{";
        json += "\"wifiSSID\":\"" + getWifiSSID() + "\",";
        json += "\"wifiPass\":\"" + getWifiPassword() + "\",";
        json += "\"clientID\":\"" + getClientID() + "\",";
        json += "\"apiUrl\":\"" + getApiUrl() + "\"";
        json += "}";
        request->send(200, "application/json", json);
    });

    // Endpoint: Update Device Configuration
    server.on("/saveConfig", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            if (!request->authenticate(WEB_USERNAME, WEB_PASSWORD)) {
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

            idx = json.indexOf("\"clientID\":\"") + 12;
            String newClientID = json.substring(idx, json.indexOf("\"", idx));

            idx = json.indexOf("\"apiUrl\":\"") + 10;
            String newApiUrl = json.substring(idx, json.indexOf("\"", idx));

            updateConfig(newSsid, newPass, newClientID, newApiUrl);
            request->send(200, "text/plain", "OK");
            delay(2000);
            ESP.restart();
        }
    );

    // Endpoint: Execute System Commands
    server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->authenticate(WEB_USERNAME, WEB_PASSWORD)) {
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
    server.on("/resetConfig", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->authenticate(WEB_USERNAME, WEB_PASSWORD)) {
            return request->requestAuthentication();
        }

        Preferences pref;
        pref.begin("sensor-config", false);
        pref.clear();
        pref.end();

        request->send(200, "text/plain", "Configuration reset! Restarting with default settings...");
        delay(2000);
        ESP.restart();
    });

    // Endpoint: OTA Firmware Upload Handler
    server.on("/doUpdate", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            if (!request->authenticate(WEB_USERNAME, WEB_PASSWORD)) {
                return request->requestAuthentication();
            }
            bool success = !Update.hasError();
            AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", success ? "OK" : "FAIL");
            response->addHeader("Connection", "close");
            request->send(response);
            delay(1000);
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

    server.begin();
    setWebLogReady();
    Serial.println("Web Server started on port " + String(WEB_PORT));
    Serial.println("Open: http://" + WiFi.localIP().toString() + ":" + String(WEB_PORT));
}
