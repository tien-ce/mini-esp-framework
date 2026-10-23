#include "services/web/web_routes_api.h"
#include "services/web/web_auth.h"
#include "services/web/web_template.h"
#include "services/wifi/wifi_task.h"
#include "services/mqtt/mqtt_task.h"
#include "core/pin_config.h"
#include "core/dispatcher.h"
#include "core/core_log.h"
#include "services/logger/log_task.h"
#include "config.h"
#include <Arduino.h>
#include <ArduinoJson.h>


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
        postIncomingCommand(CMD_RESTART);
        vTaskDelete(NULL);
    }, "save_restart_task", 4096, NULL, 1, NULL);
}

/** @brief Handles HTTP GET request for sensor telemetry data ("/api/telemetry"). */
static void handleTelemetry(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;

    dispatch_signal_sync(SIG_WEB_POLL);
    request->send(200, "application/json", web_get_telemetry_json());
    web_clear_telemetry_json();
}

/** @brief Handles HTTP GET request for the GPIO pin mapping as JSON ("/api/gpio"). */
static void handleGpio(AsyncWebServerRequest *request) {
    // Checking authentication
    if (!web_authenticate(request)) return;

    // Create a stream response and fill it directly (no intermediate String)
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    if (response == nullptr) {
        request->send(500, "text/plain", "Out of memory");
        return;
    }
    web_build_gpio_json(*response);
    request->send(response);
}

/** @brief Handles HTTP GET request for the registered console command list ("/api/commands"). */
static void handleCommands(AsyncWebServerRequest *request) {
    // Checking authentication
    if (!web_authenticate(request)) return;

    // Stream the JSON list straight into the response
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    if (response == nullptr) {
        request->send(500, "text/plain", "Out of memory");
        return;
    }
    cmd_list_json(*response);
    request->send(response);
}

/** @brief Handles HTTP GET request for the stored Wi-Fi settings as JSON ("/api/config/wifi"). The password is never returned. */
static void handleConfigWifi(AsyncWebServerRequest *request) {
    // Checking authentication
    if (!web_authenticate(request)) return;

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    if (response == nullptr) {
        request->send(500, "text/plain", "Out of memory");
        return;
    }

    // Build the document from the stored settings (static IP fields are the saved values, not the live lease)
    JsonDocument doc;
    doc["client_id"]    = getWifiClientID();
    doc["ssid"]         = getWifiSSID();
    doc["ip"]           = getWifiIP();
    doc["gw"]           = getWifiGateway();
    doc["sn"]           = getWifiSubnet();
    doc["dns"]          = getWifiDNS1();
    doc["has_password"] = getWifiPassword().length() > 0;

    serializeJson(doc, *response);
    request->send(response);
}

/** @brief Handles HTTP GET request for the stored MQTT settings as JSON ("/api/config/mqtt"). The password is never returned. */
static void handleConfigMqtt(AsyncWebServerRequest *request) {
    // Checking authentication
    if (!web_authenticate(request)) return;

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    if (response == nullptr) {
        request->send(500, "text/plain", "Out of memory");
        return;
    }

    JsonDocument doc;
    doc["server"]       = getMqttServer();
    doc["port"]         = getMqttPort();
    doc["user"]         = getMqttUser();
    doc["topic"]        = getMqttDataTopic();
    doc["rpc_topic"]    = getMqttRpcTopic();
    doc["interval"]     = getMqttInterval();
    doc["has_password"] = getMqttPass().length() > 0;

    serializeJson(doc, *response);
    request->send(response);
}

/** @brief Handles HTTP GET request for live system diagnostic and network statistics ("/stats"). */
static void handleStats(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;

    String json = "{";
    json += "\"uptime\":" + String(millis() / 1000) + ",";
    json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"gw\":\"" + WiFi.gatewayIP().toString() + "\",";
    json += "\"mask\":\"" + WiFi.subnetMask().toString() + "\",";
    json += "\"dns1\":\"" + WiFi.dnsIP().toString() + "\"";
    json += "}";
    request->send(200, "application/json", json);
}

/** @brief Handles HTTP POST form submission for complete WiFi and Static IP settings ("/saveWifi"). */
static void handleSaveWifi(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;

    String ssid      = request->hasParam("ssid", true)      ? request->getParam("ssid", true)->value() : getWifiSSID();
    // An empty or missing password keeps the stored one (the password is never sent to the browser)
    String pass      = request->hasParam("pass", true)      ? request->getParam("pass", true)->value() : String();
    if (pass.length() == 0) pass = getWifiPassword();
    String clientId  = request->hasParam("client_id", true) ? request->getParam("client_id", true)->value() : getWifiClientID();
    String ip        = request->hasParam("ip", true)        ? request->getParam("ip", true)->value() : getWifiIP();
    String gw        = request->hasParam("gw", true)        ? request->getParam("gw", true)->value() : getWifiGateway();
    String sn        = request->hasParam("sn", true)        ? request->getParam("sn", true)->value() : getWifiSubnet();
    String dns       = request->hasParam("dns", true)       ? request->getParam("dns", true)->value() : getWifiDNS1();

    request->send(200, "text/plain", "OK");

    struct WifiFullSaveArgs {
        String ssid;
        String pass;
        String clientId;
        String ip;
        String gw;
        String sn;
        String dns;
    };
    WifiFullSaveArgs *args = new WifiFullSaveArgs{ssid, pass, clientId, ip, gw, sn, dns};

    xTaskCreate([](void *arg) {
        WifiFullSaveArgs *a = (WifiFullSaveArgs*)arg;
        updateWifiConfig(a->ssid, a->pass);
        updateWifiClientID(a->clientId);
        updateWifiStaticIPConfig(a->ip, a->gw, a->sn, a->dns);
        delete a;
        postIncomingCommand(CMD_RESTART);
        vTaskDelete(NULL);
    }, "save_wifi_task", 4096, args, 1, NULL);
}

/** @brief Handles HTTP POST form submission for MQTT broker settings ("/saveMqtt"). */
static void handleSaveMqtt(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;

    String server   = request->hasParam("server", true)    ? request->getParam("server", true)->value() : getMqttServer();
    uint16_t port   = request->hasParam("port", true)      ? (uint16_t)request->getParam("port", true)->value().toInt() : getMqttPort();
    String user     = request->hasParam("user", true)      ? request->getParam("user", true)->value() : getMqttUser();
    // An empty or missing password keeps the stored one (the password is never sent to the browser)
    String pass     = request->hasParam("pass", true)      ? request->getParam("pass", true)->value() : String();
    if (pass.length() == 0) pass = getMqttPass();
    String topic    = request->hasParam("topic", true)     ? request->getParam("topic", true)->value() : getMqttDataTopic();
    String rpcTopic = request->hasParam("rpc_topic", true) ? request->getParam("rpc_topic", true)->value() : getMqttRpcTopic();
    uint32_t interval = request->hasParam("interval", true) ? (uint32_t)request->getParam("interval", true)->value().toInt() : getMqttInterval();

    request->send(200, "text/plain", "OK");

    struct MqttSaveArgs {
        String server;
        uint16_t port;
        String user;
        String pass;
        uint32_t interval;
        String topic;
        String rpcTopic;
    };
    MqttSaveArgs *args = new MqttSaveArgs{server, port, user, pass, interval, topic, rpcTopic};

    xTaskCreate([](void *arg) {
        MqttSaveArgs *a = (MqttSaveArgs*)arg;
        updateMqttConfig(a->server, a->port, a->user, a->pass, a->interval, a->topic, a->rpcTopic);
        delete a;
        postIncomingCommand(CMD_RESTART);
        vTaskDelete(NULL);
    }, "save_mqtt_task", 4096, args, 1, NULL);
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
    server->on("/saveWifi", HTTP_POST, handleSaveWifi);
    server->on("/saveMqtt", HTTP_POST, handleSaveMqtt);
    server->on("/api/telemetry", HTTP_GET, handleTelemetry);
    server->on("/api/gpio", HTTP_GET, handleGpio);
    server->on("/api/commands", HTTP_GET, handleCommands);
    server->on("/api/config/wifi", HTTP_GET, handleConfigWifi);
    server->on("/api/config/mqtt", HTTP_GET, handleConfigMqtt);
    server->on("/stats", HTTP_GET, handleStats);
}

