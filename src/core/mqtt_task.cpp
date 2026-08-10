#include "core/mqtt_task.h"
#include "core/config_manager.h"
#include "core/core_engine.h"
#include "core/dispatcher.h"
#include "core/log_task.h"
#include "core/info.h"
#include "config.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include <semphr.h>

/* -------------------------------------------------------------------------- */
/*                              STATIC VARIABLES                              */
/* -------------------------------------------------------------------------- */

static String mqtt_server;
static uint16_t mqtt_port;
static String mqtt_user;
static String mqtt_pass;
static uint32_t mqtt_interval;
static String mqtt_data_topic;
static String mqtt_rpc_topic;

static WiFiClient espClient;
static PubSubClient mqttClient(espClient);
static SemaphoreHandle_t mqttMutex = NULL;

static JsonDocument mqttDoc;

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

static void initMqttMutex() {
    if (mqttMutex == NULL) {
        mqttMutex = xSemaphoreCreateMutex();
    }
}

static void saveMqttConfig() {
    String content = "";
    if (mqttMutex != NULL && xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
        content += "server=" + mqtt_server + "\n";
        content += "port=" + String(mqtt_port) + "\n";
        content += "user=" + mqtt_user + "\n";
        content += "pass=" + mqtt_pass + "\n";
        content += "interval=" + String(mqtt_interval) + "\n";
        content += "data_topic=" + mqtt_data_topic + "\n";
        content += "rpc_topic=" + mqtt_rpc_topic + "\n";
        xSemaphoreGive(mqttMutex);
    }
    save_config("mqtt", content);
}

static void loadMqttConfig() {
    register_config_file("mqtt", "mqtt_config.txt");
    initMqttMutex();

    String raw = read_config("mqtt");
    if (raw.length() == 0) {
        LOG_INFO("mqtt config file not found or empty. Creating default mqtt_config.txt");
        mqtt_server     = MQTT_SERVER;
        mqtt_port       = MQTT_PORT;
        mqtt_user       = MQTT_USER;
        mqtt_pass       = MQTT_PASS;
        mqtt_interval   = MQTT_INTERVAL;
        mqtt_data_topic = MQTT_DATA_TOPIC;
        mqtt_rpc_topic  = MQTT_RPC_TOPIC;
        saveMqttConfig();
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

            if (key.equalsIgnoreCase("server")) mqtt_server = val;
            else if (key.equalsIgnoreCase("port")) mqtt_port = (uint16_t)val.toInt();
            else if (key.equalsIgnoreCase("user")) mqtt_user = val;
            else if (key.equalsIgnoreCase("pass")) mqtt_pass = val;
            else if (key.equalsIgnoreCase("interval")) mqtt_interval = (uint32_t)val.toInt();
            else if (key.equalsIgnoreCase("data_topic")) mqtt_data_topic = val;
            else if (key.equalsIgnoreCase("rpc_topic")) mqtt_rpc_topic = val;
        }
    }
    LOG_INFO("mqtt config loaded successfully.");
}

static void connectBroker() {
    if (mqttClient.connected()) return;

    String clientId = String(esp_info_get_model()) + "_" + String(esp_info_get_mac_str());
    mqttClient.setServer(mqtt_server.c_str(), mqtt_port);

    LOG_INFO("Connecting to MQTT Broker: " + mqtt_server);

    bool status = false;
    if (mqtt_user.length() > 0) {
        status = mqttClient.connect(clientId.c_str(), mqtt_user.c_str(), mqtt_pass.c_str());
    } else {
        status = mqttClient.connect(clientId.c_str());
    }

    if (status) {
        LOG_INFO("MQTT Broker connected successfully.");
        if (mqtt_rpc_topic.length() > 0) {
            mqttClient.subscribe(mqtt_rpc_topic.c_str());
            LOG_INFO("Subscribed to RPC Topic: " + mqtt_rpc_topic);
        }
    } else {
        LOG_ERROR("MQTT connection failed, rc=" + String(mqttClient.state()));
    }
}


/* -------------------------------------------------------------------------- */
/*                              PUBLIC CONFIG APIS                            */
/* -------------------------------------------------------------------------- */

String getMqttServer() { return mqtt_server; }
uint16_t getMqttPort() { return mqtt_port; }
String getMqttUser() { return mqtt_user; }
String getMqttPass() { return mqtt_pass; }
uint32_t getMqttInterval() { return mqtt_interval; }
String getMqttDataTopic() { return mqtt_data_topic; }
String getMqttRpcTopic() { return mqtt_rpc_topic; }

void updateMqttConfig(const String &server, uint16_t port, const String &user,
                      const String &pass, uint32_t interval, const String &dataTopic,
                      const String &rpcTopic) {
    if (mqttMutex != NULL && xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
        mqtt_server = server;
        mqtt_port = port;
        mqtt_user = user;
        mqtt_pass = pass;
        mqtt_interval = interval;
        mqtt_data_topic = dataTopic;
        mqtt_rpc_topic = rpcTopic;
        xSemaphoreGive(mqttMutex);
    }
    saveMqttConfig();
}


void vMqttTask(void *pvParameters) {
    waiting_on_event(NETWORK_EVENT, NET_STATE_WIFI_STA, portMAX_DELAY);
    
    loadMqttConfig();

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        if (is_wifi_connected()) {
            if (!mqttClient.connected()) {
                connectBroker();
            }

            mqttClient.loop();

            if (mqttClient.connected()) {
                if (mqttMutex != NULL && xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
                    mqttDoc.clear();

                    mqttDoc["clientID"] = String(esp_info_get_model()) + "_" + String(esp_info_get_mac_str());
                    mqttDoc["ip"]       = getWifiSSID();
                    mqttDoc["rssi"]     = WiFi.RSSI();
                    mqttDoc["freeHeap"] = ESP.getFreeHeap();
                    mqttDoc["uptime"]   = millis() / 1000;

                    xSemaphoreGive(mqttMutex);
                }

                // Core engine dispatches signal for driver data population
                dispatch_signal(SIG_MQTT_PUBLISH);

                if (mqttMutex != NULL && xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
                    String jsonBuffer;
                    serializeJson(mqttDoc, jsonBuffer);

                    if (mqtt_data_topic.length() > 0) {
                        mqttClient.publish(mqtt_data_topic.c_str(), jsonBuffer.c_str());
                    }
                    xSemaphoreGive(mqttMutex);
                }
            }
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(mqtt_interval));
    }
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC DATA API                               */
/* -------------------------------------------------------------------------- */

template <typename T>
void mqtt_add_telemetry(const String &key, T value) {
    mqttDoc[key] = value;
    LOG_DEBUG("Added telemetry: " + key + " = " + String(value));
}

template void mqtt_add_telemetry<int>(const String&, int);
template void mqtt_add_telemetry<float>(const String&, float);
template void mqtt_add_telemetry<double>(const String&, double);
template void mqtt_add_telemetry<char*>(const String&, char*);
template void mqtt_add_telemetry<const char*>(const String&, const char*);
template void mqtt_add_telemetry<String>(const String&, String);
template void mqtt_add_telemetry<bool>(const String&, bool);
