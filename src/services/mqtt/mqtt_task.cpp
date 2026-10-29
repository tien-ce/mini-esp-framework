#include "services/mqtt/mqtt_task.h"
#include "services/logger/log_task.h"
#include "core/core_nvs.h"
#include "core/core_engine.h"
#include "core/dispatcher.h"
#include "core/core_log.h"
#include "core/info.h"
#include "services/wifi/wifi_task.h"
#include "config.h"
#include "cmd.h"

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

/** Max time a caller waits for mqttMutex in mqtt_add_telemetry(). */
#define MQTT_TELEMETRY_LOCK_TIMEOUT_MS 1000

/** PubSubClient packet buffer size; must exceed topic + serialized telemetry JSON. */
#define MQTT_PACKET_BUFFER_SIZE 2048

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

static void saveMqttConfig() {
    if (mqttMutex != NULL && xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
        {
            core_nvs_save_string("mqtt", "server", mqtt_server);
            core_nvs_save_int("mqtt", "port", mqtt_port);
            core_nvs_save_string("mqtt", "user", mqtt_user);
            core_nvs_save_string("mqtt", "pass", mqtt_pass);
            core_nvs_save_int("mqtt", "interval", mqtt_interval);
            core_nvs_save_string("mqtt", "data_topic", mqtt_data_topic);
            core_nvs_save_string("mqtt", "rpc_topic", mqtt_rpc_topic);
            /* lock released */
        }
        xSemaphoreGive(mqttMutex);
    }
}

static void loadMqttConfig() {
    core_nvs_register_namespace("mqtt");

    if (mqttMutex != NULL && xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
        {
            mqtt_server     = core_nvs_read_string("mqtt", "server", MQTT_SERVER);
            mqtt_port       = (uint16_t)core_nvs_read_int("mqtt", "port", MQTT_PORT);
            mqtt_user       = core_nvs_read_string("mqtt", "user", MQTT_USER);
            mqtt_pass       = core_nvs_read_string("mqtt", "pass", MQTT_PASS);
            mqtt_interval   = (uint32_t)core_nvs_read_int("mqtt", "interval", MQTT_INTERVAL);
            mqtt_data_topic = core_nvs_read_string("mqtt", "data_topic", MQTT_DATA_TOPIC);
            mqtt_rpc_topic  = core_nvs_read_string("mqtt", "rpc_topic", MQTT_RPC_TOPIC);
            /* lock released */
        }
        xSemaphoreGive(mqttMutex);
    }
    LOG_INFO("mqtt config loaded from NVS successfully.");
}

static void connectBroker() {
    if (mqttClient.connected()) return;

    String clientId = String(esp_info_get_model()) + "_" + String(esp_info_get_mac_str());
    mqttClient.setServer(mqtt_server.c_str(), mqtt_port);
    /* PubSubClient defaults to a 256-byte packet buffer; larger publishes are silently dropped */
    if (!mqttClient.setBufferSize(MQTT_PACKET_BUFFER_SIZE)) {
        LOG_ERROR("Failed to allocate MQTT packet buffer");
    }

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

/** @brief Command handler: Gets or sets MQTT publish data topic. */
static void mqtt_cmd_data_topic(const String &arg) {
    String cleanVal = arg;
    cleanVal.trim();
    while (cleanVal.endsWith(";")) {
        cleanVal = cleanVal.substring(0, cleanVal.length() - 1);
        cleanVal.trim();
    }
    if (cleanVal.length() > 0) {
        if (mqttMutex != NULL && xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
            mqtt_data_topic = cleanVal;
            xSemaphoreGive(mqttMutex);
        } else {
            mqtt_data_topic = cleanVal;
        }
        saveMqttConfig();
        LOG_INFO("MQTT Data Topic updated to: " + mqtt_data_topic);
    } else {
        LOG_INFO("MQTT Data Topic: " + getMqttDataTopic());
    }
}

/** @brief Command handler: Gets or sets MQTT RPC topic. */
static void mqtt_cmd_rpc_topic(const String &arg) {
    String cleanVal = arg;
    cleanVal.trim();
    while (cleanVal.endsWith(";")) {
        cleanVal = cleanVal.substring(0, cleanVal.length() - 1);
        cleanVal.trim();
    }
    if (cleanVal.length() > 0) {
        if (mqttMutex != NULL && xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
            mqtt_rpc_topic = cleanVal;
            xSemaphoreGive(mqttMutex);
        } else {
            mqtt_rpc_topic = cleanVal;
        }
        saveMqttConfig();
        LOG_INFO("MQTT RPC Topic updated to: " + mqtt_rpc_topic);
    } else {
        LOG_INFO("MQTT RPC Topic: " + getMqttRpcTopic());
    }
}

/** @brief Command handler: Gets or sets MQTT broker server address. */
static void mqtt_cmd_server(const String &arg) {
    String cleanVal = arg;
    cleanVal.trim();
    while (cleanVal.endsWith(";")) {
        cleanVal = cleanVal.substring(0, cleanVal.length() - 1);
        cleanVal.trim();
    }
    if (cleanVal.length() > 0) {
        if (mqttMutex != NULL && xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
            mqtt_server = cleanVal;
            xSemaphoreGive(mqttMutex);
        } else {
            mqtt_server = cleanVal;
        }
        saveMqttConfig();
        LOG_INFO("MQTT Server updated to: " + mqtt_server);
    } else {
        LOG_INFO("MQTT Server: " + getMqttServer());
    }
}

/** @brief Command handler: Gets or sets MQTT broker port. */
static void mqtt_cmd_port(const String &arg) {
    String cleanVal = arg;
    cleanVal.trim();
    while (cleanVal.endsWith(";")) {
        cleanVal = cleanVal.substring(0, cleanVal.length() - 1);
        cleanVal.trim();
    }
    if (cleanVal.length() > 0) {
        int port = cleanVal.toInt();
        if (port <= 0 || port > 65535) {
            LOG_ERROR("Invalid MQTT port: " + cleanVal);
            return;
        }
        if (mqttMutex != NULL && xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
            mqtt_port = (uint16_t)port;
            xSemaphoreGive(mqttMutex);
        } else {
            mqtt_port = (uint16_t)port;
        }
        saveMqttConfig();
        LOG_INFO("MQTT Port updated to: " + String(mqtt_port));
    } else {
        LOG_INFO("MQTT Port: " + String(getMqttPort()));
    }
}

/** @brief Command handler: Gets or sets MQTT username. */
static void mqtt_cmd_user(const String &arg) {
    String cleanVal = arg;
    cleanVal.trim();
    while (cleanVal.endsWith(";")) {
        cleanVal = cleanVal.substring(0, cleanVal.length() - 1);
        cleanVal.trim();
    }
    if (cleanVal.length() > 0) {
        if (mqttMutex != NULL && xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
            mqtt_user = cleanVal;
            xSemaphoreGive(mqttMutex);
        } else {
            mqtt_user = cleanVal;
        }
        saveMqttConfig();
        LOG_INFO("MQTT User updated to: " + mqtt_user);
    } else {
        LOG_INFO("MQTT User: " + getMqttUser());
    }
}

/** @brief Command handler: Gets or sets MQTT password. */
static void mqtt_cmd_password(const String &arg) {
    String cleanVal = arg;
    cleanVal.trim();
    while (cleanVal.endsWith(";")) {
        cleanVal = cleanVal.substring(0, cleanVal.length() - 1);
        cleanVal.trim();
    }
    if (cleanVal.length() > 0) {
        if (mqttMutex != NULL && xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
            mqtt_pass = cleanVal;
            xSemaphoreGive(mqttMutex);
        } else {
            mqtt_pass = cleanVal;
        }
        saveMqttConfig();
        LOG_INFO("MQTT Password updated.");
    } else {
        LOG_INFO("MQTT Password: " + getMqttPass());
    }
}

/** @brief Command handler: Gets or sets MQTT telemetry publish interval in ms. */
static void mqtt_cmd_interval(const String &arg) {
    String cleanVal = arg;
    cleanVal.trim();
    while (cleanVal.endsWith(";")) {
        cleanVal = cleanVal.substring(0, cleanVal.length() - 1);
        cleanVal.trim();
    }
    if (cleanVal.length() > 0) {
        int interval = cleanVal.toInt();
        if (interval < 500) {
            LOG_ERROR("MQTT Interval too low (minimum 500ms): " + cleanVal);
            return;
        }
        if (mqttMutex != NULL && xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
            mqtt_interval = (uint32_t)interval;
            xSemaphoreGive(mqttMutex);
        } else {
            mqtt_interval = (uint32_t)interval;
        }
        saveMqttConfig();
        LOG_INFO("MQTT Interval updated to: " + String(mqtt_interval) + " ms");
    } else {
        LOG_INFO("MQTT Interval: " + String(getMqttInterval()) + " ms");
    }
}

/** @brief Command handler: Displays full MQTT configuration and connection status. */
static void mqtt_cmd_status(const String &arg) {
    LOG_INFO("--- MQTT Configuration & Status ---");
    LOG_INFO("Server: " + getMqttServer() + ":" + String(getMqttPort()));
    LOG_INFO("User: " + getMqttUser());
    LOG_INFO("Connected: " + String(mqttClient.connected() ? "YES" : "NO"));
    LOG_INFO("Data Topic: " + getMqttDataTopic());
    LOG_INFO("RPC Topic: " + getMqttRpcTopic());
    LOG_INFO("Interval: " + String(getMqttInterval()) + " ms");
    LOG_INFO("------------------------------------");
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


/**
 * @brief Create the MQTT mutex; must run once before any task using the MQTT API starts.
 * @return true if the mutex exists after the call, false on allocation failure.
 */
bool mqtt_init() {
    if (mqttMutex == NULL) {
        mqttMutex = xSemaphoreCreateMutex();
    }
    return mqttMutex != NULL;
}

/**
 * @brief MQTT task: connect to the broker, collect telemetry and publish periodically.
 * @param[in] pvParameters Unused.
 */
void vMqttTask(void *pvParameters) {
    core_state_wait(g_wifi_state_group, (uint8_t)WIFI_STATE_STA, portMAX_DELAY);
    
    loadMqttConfig();

    // Register MQTT CLI / Web commands
    register_cmd(CMD_MQTT_DATA_TOPIC, mqtt_cmd_data_topic);
    register_cmd("MQTT_DATA_TOPIC", mqtt_cmd_data_topic);
    register_cmd("MQTT_TOPIC", mqtt_cmd_data_topic);

    register_cmd(CMD_MQTT_RPC_TOPIC, mqtt_cmd_rpc_topic);
    register_cmd("DATA_RPC", mqtt_cmd_rpc_topic);
    register_cmd("MQTT_RPC_TOPIC", mqtt_cmd_rpc_topic);
    register_cmd("MQTT_RPC", mqtt_cmd_rpc_topic);
    register_cmd("RPC_TOPIC", mqtt_cmd_rpc_topic);

    register_cmd(CMD_MQTT_SERVER, mqtt_cmd_server);
    register_cmd("MQTT_HOST", mqtt_cmd_server);
    register_cmd("BROKER", mqtt_cmd_server);

    register_cmd(CMD_MQTT_PORT, mqtt_cmd_port);

    register_cmd(CMD_MQTT_USER, mqtt_cmd_user);
    register_cmd("MQTT_USERNAME", mqtt_cmd_user);

    register_cmd(CMD_MQTT_PASSWORD, mqtt_cmd_password);
    register_cmd("MQTT_PASS", mqtt_cmd_password);

    register_cmd(CMD_MQTT_INTERVAL, mqtt_cmd_interval);

    register_cmd(CMD_MQTT_STATUS, mqtt_cmd_status);

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        if (is_wifi_connected()) {
            if (!mqttClient.connected()) {
                connectBroker();
            }

            mqttClient.loop();

            if (mqttClient.connected()) {
                /* Step 1: Refresh system metadata under the mutex. The document is NOT cleared:
                 * script/driver values pushed via mqtt_add_telemetry() between publishes must
                 * survive until the next publish; each key is simply overwritten on update. */
                if (mqttMutex != NULL && xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
                    String clientID = getWifiClientID();
                    mqttDoc["clientID"] = clientID.length() > 0 ? clientID : (String(esp_info_get_model()) + "_" + String(esp_info_get_mac_str()));
                    mqttDoc["ip"]       = WiFi.localIP().toString();
                    mqttDoc["rssi"]     = WiFi.RSSI();
                    mqttDoc["freeHeap"] = ESP.getFreeHeap();
                    mqttDoc["uptime"]   = millis() / 1000;

                    /* Release the mutex before dispatching: drivers call mqtt_add_telemetry(),
                     * which takes it again (avoids deadlock and keeps the critical section short). */
                    xSemaphoreGive(mqttMutex);
                }

                /* Step 2: Let drivers append their telemetry via mqtt_add_telemetry() */
                dispatch_signal_sync(SIG_MQTT_PUBLISH);

                /* Step 3: Re-take the mutex, serialize the payload and publish it */
                if (mqttMutex != NULL && xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
                    String jsonBuffer;
                    serializeJson(mqttDoc, jsonBuffer);
                    LOG_INFO_STR("Mqtt publish: %s",jsonBuffer.c_str());

                    if (mqtt_data_topic.length() > 0) {
                        if (!mqttClient.publish(mqtt_data_topic.c_str(), jsonBuffer.c_str())) {
                            LOG_ERROR("MQTT publish failed, payload bytes=" + String(jsonBuffer.length()));
                        }
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
bool mqtt_add_telemetry(const String &key, T value) {
    /* Step 1: Reject the call if mqtt_init() has not created the mutex yet */
    if (mqttMutex == NULL) return false;

    /* Step 2: Write the pair into the shared document under the mutex */
    if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(MQTT_TELEMETRY_LOCK_TIMEOUT_MS)) != pdTRUE) {
        LOG_ERROR("mqtt_add_telemetry: mutex timeout for key " + key);
        return false;
    }
    mqttDoc[key] = value;
    xSemaphoreGive(mqttMutex);

    LOG_DEBUG("Added telemetry: " + key);
    return true;
}

template bool mqtt_add_telemetry<int>(const String&, int);
template bool mqtt_add_telemetry<float>(const String&, float);
template bool mqtt_add_telemetry<double>(const String&, double);
template bool mqtt_add_telemetry<char*>(const String&, char*);
template bool mqtt_add_telemetry<const char*>(const String&, const char*);
template bool mqtt_add_telemetry<String>(const String&, String);
template bool mqtt_add_telemetry<bool>(const String&, bool);

bool is_mqtt_connected() {
    return mqttClient.connected();
}

void mqtt_task_start() {
    mqtt_init();
    xTaskCreatePinnedToCore(
        vMqttTask,
        "MqttTask",
        6144,
        NULL,
        3,
        NULL,
        1
    );
}

