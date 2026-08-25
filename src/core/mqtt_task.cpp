#include "core/mqtt_task.h"
#include "core/config_manager.h"
#include "core/core_engine.h"
#include "core/dispatcher.h"
#include "core/log_task.h"
#include "core/info.h"
#include "core/wifi_task.h"
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
    if (mqttMutex != NULL && xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
        if (config_get_lock()) {
            config_save_string("mqtt", "server", mqtt_server);
            config_save_int("mqtt", "port", mqtt_port);
            config_save_string("mqtt", "user", mqtt_user);
            config_save_string("mqtt", "pass", mqtt_pass);
            config_save_int("mqtt", "interval", mqtt_interval);
            config_save_string("mqtt", "data_topic", mqtt_data_topic);
            config_save_string("mqtt", "rpc_topic", mqtt_rpc_topic);
            config_release_lock();
        }
        xSemaphoreGive(mqttMutex);
    }
}

static void loadMqttConfig() {
    initMqttMutex();
    register_config_module("mqtt", "mqtt");

    if (mqttMutex != NULL && xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
        if (config_get_lock()) {
            mqtt_server     = config_read_string("mqtt", "server", MQTT_SERVER);
            mqtt_port       = (uint16_t)config_read_int("mqtt", "port", MQTT_PORT);
            mqtt_user       = config_read_string("mqtt", "user", MQTT_USER);
            mqtt_pass       = config_read_string("mqtt", "pass", MQTT_PASS);
            mqtt_interval   = (uint32_t)config_read_int("mqtt", "interval", MQTT_INTERVAL);
            mqtt_data_topic = config_read_string("mqtt", "data_topic", MQTT_DATA_TOPIC);
            mqtt_rpc_topic  = config_read_string("mqtt", "rpc_topic", MQTT_RPC_TOPIC);
            config_release_lock();
        }
        xSemaphoreGive(mqttMutex);
    }
    LOG_INFO("mqtt config loaded from NVS successfully.");
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

                    String clientID = getWifiClientID();
                    mqttDoc["clientID"] = clientID.length() > 0 ? clientID : (String(esp_info_get_model()) + "_" + String(esp_info_get_mac_str()));
                    mqttDoc["ip"]       = WiFi.localIP().toString();
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
