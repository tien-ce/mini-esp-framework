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
#include <semphr.h>
#include <esp_mac.h>

#ifdef USE_MQTT
#include <PubSubClient.h>

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

/** How often mqttClient.loop() runs: must stay well under the keepalive so pings go out on time. */
#define MQTT_LOOP_PERIOD_MS      100

/** Minimum gap between two connection attempts while the broker is unreachable. */
#define MQTT_RECONNECT_PERIOD_MS 5000

/* ---------------------------- CENTRAL MONITOR ----------------------------- */
/*
 * Every device publishes the same topic tree for the central monitor, whatever its app does
 * (the app's own data/RPC topics above are separate and untouched):
 *
 *   IOT-MONITOR/<DEVICE_ID>/STATUS   "online" / "offline"            retained
 *   IOT-MONITOR/<DEVICE_ID>/INFO     {"name","mac","ip","fw",...}    retained
 *   IOT-MONITOR/<DEVICE_ID>/ALARM/<CODE>, /DATA, /ATTR                (see below)
 *
 * <DEVICE_ID> is the Wi-Fi MAC without ':' (e.g. 3C84279A1B20): fixed and unique per chip, so it
 * never depends on a name typed by hand. The human-readable name travels inside INFO.
 * Retained: the broker keeps the last value, so a monitor opened later gets it immediately.
 */
#define MONITOR_TOPIC_ROOT       "IOT-MONITOR"
#define MONITOR_TOPIC_STATUS     "STATUS"
#define MONITOR_TOPIC_INFO       "INFO"

/* STATUS payloads */
#define MONITOR_STATUS_ONLINE    "online"   /* Published by the device right after connecting */
#define MONITOR_STATUS_OFFLINE   "offline"  /* Will: published BY THE BROKER when the device dies */

/* Will QoS 1: the broker must not lose the "offline" message */
#define MONITOR_WILL_QOS         1

#define MONITOR_DEVICE_ID_LEN    13         /* 12 hex digits + NUL */
#define MONITOR_TOPIC_MAX_LEN    64         /* "IOT-MONITOR/" + DEVICE_ID + "/" + channel + NUL */

/*
 * Alarms: IOT-MONITOR/<DEVICE_ID>/ALARM/<CODE>   {"msg","source","level","active"}   retained
 * Pushed by any task through mqtt_alarm_publish(), which only queues a copy; the MQTT task
 * (the only user of mqttClient) publishes them. The monitor deletes the topic when someone
 * acknowledges the alarm.
 */
#define MONITOR_TOPIC_ALARM      "ALARM"
#define MQTT_ALARM_QUEUE_LEN     16         /* Alarms waiting while the broker is unreachable */
#define MQTT_ALARM_CODE_LEN      24         /* Including NUL; also the last topic level */
#define MQTT_ALARM_MSG_LEN       96
#define MQTT_ALARM_SOURCE_LEN    24
#define MQTT_ALARM_LEVEL_LEN     16         /* Free text chosen by the caller, e.g. "warning", "critical" */

/* One queued alarm, copied by value into the queue (no malloc) */
typedef struct {
    char code[MQTT_ALARM_CODE_LEN];
    char msg[MQTT_ALARM_MSG_LEN];
    char source[MQTT_ALARM_SOURCE_LEN];
    char level[MQTT_ALARM_LEVEL_LEN];
    bool active;
} mqtt_alarm_t;

static QueueHandle_t alarmQueue = NULL;     /* Created in mqtt_init() */

/*
 * Data: IOT-MONITOR/<DEVICE_ID>/DATA   same JSON as the app data topic, not retained.
 * Published right after the app data topic, every mqtt_interval.
 */
#define MONITOR_TOPIC_DATA       "DATA"

/*
 * Attributes: IOT-MONITOR/<DEVICE_ID>/ATTR   {"key": value, ...}   retained
 * Values that rarely change (clientID, ip, serial, setpoint...), so they are not resent with
 * every telemetry payload. mqtt_set_attribute() only stores the value, and raises attrDirty
 * when it differs from the stored one. Every MQTT_ATTR_CHECK_PERIOD_MS the MQTT task tests the
 * flag (no scan of the attributes) and, if raised, publishes ALL attributes in one message:
 * a retained topic keeps only its last message, so sending the changed key alone would make
 * the broker forget the others.
 */
#define MONITOR_TOPIC_ATTR       "ATTR"
#define MQTT_ATTR_CHECK_PERIOD_MS 1000

static JsonDocument attrDoc;                /* Guarded by mqttMutex */
static bool attrDirty = false;              /* Guarded by mqttMutex: attrDoc changed since last publish */

/* Built once in vMqttTask() from the MAC; the MAC never changes, so neither do the topics */
static char monitor_device_id[MONITOR_DEVICE_ID_LEN];
static char monitor_status_topic[MONITOR_TOPIC_MAX_LEN];   /* IOT-MONITOR/<DEVICE_ID>/STATUS */
static char monitor_info_topic[MONITOR_TOPIC_MAX_LEN];     /* IOT-MONITOR/<DEVICE_ID>/INFO */
static char monitor_data_topic[MONITOR_TOPIC_MAX_LEN];     /* IOT-MONITOR/<DEVICE_ID>/DATA */
static char monitor_attr_topic[MONITOR_TOPIC_MAX_LEN];     /* IOT-MONITOR/<DEVICE_ID>/ATTR */

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

/**
 * @brief Opens the MQTT session, then announces the device to the central monitor.
 *
 * Order matters:
 *   1. connect() carries the Will (STATUS = "offline"). The Will is part of the CONNECT packet,
 *      not a normal publish, so it must be handed over here, before the session exists.
 *   2. Once connected: publish STATUS = "online" and INFO (both retained). These are normal
 *      publishes, so they can only go out after connect() succeeded.
 *   3. Subscribe to the app's RPC topic.
 * Runs again after every reconnect, so the monitor always gets "online" back.
 */
static void connectBroker() {
    if (mqttClient.connected()) return;

    String clientId = String(esp_info_get_model()) + "_" + String(esp_info_get_mac_str());
    mqttClient.setServer(mqtt_server.c_str(), mqtt_port);
    /* PubSubClient defaults to a 256-byte packet buffer; larger publishes are silently dropped */
    if (!mqttClient.setBufferSize(MQTT_PACKET_BUFFER_SIZE)) {
        LOG_ERROR("Failed to allocate MQTT packet buffer");
    }

    LOG_INFO("Connecting to MQTT Broker: " + mqtt_server);

    /*
     * Step 1: Connect and hand the broker our Will.
     * If the connection later drops without a DISCONNECT (power loss, panic, hang, Wi-Fi lost),
     * the broker stops hearing our keepalive pings and publishes the Will for us:
     * STATUS = "offline" (retained), so the monitor never shows a dead board as online.
     * NULL user/pass = broker without authentication.
     */
    bool hasUser = mqtt_user.length() > 0;
    bool status = mqttClient.connect(clientId.c_str(),
                                     hasUser ? mqtt_user.c_str() : NULL,
                                     hasUser ? mqtt_pass.c_str() : NULL,
                                     monitor_status_topic,     /* Will topic */
                                     MONITOR_WILL_QOS,
                                     true,                     /* Will retained */
                                     MONITOR_STATUS_OFFLINE);  /* Will payload */

    if (status) {
        LOG_INFO("MQTT Broker connected successfully.");

        /* Step 2a: STATUS = "online" (retained), overwrites the "offline" left by the last Will */
        if (!mqttClient.publish(monitor_status_topic, MONITOR_STATUS_ONLINE, true)) {
            LOG_ERROR("MQTT monitor: STATUS publish failed");
        }

        /* Step 2b: INFO (retained): who this device is, shown and searched by name in the monitor */
        JsonDocument info;
        info["name"]         = getWifiClientID();          /* Set by whoever installs the board; may be empty */
        info["mac"]          = esp_info_get_mac_str();     /* "AA:BB:CC:DD:EE:FF" */
        info["ip"]           = WiFi.localIP().toString();
        info["fw"]           = FW_VERSION_STR;
        info["board"]        = BOARD_NAME;
        info["reset_reason"] = esp_info_get_reset_reason(); /* Why the board last restarted, e.g. "PANIC" */
        info["data_topic"]   = mqtt_data_topic;            /* Where this device's app data goes */

        String payload;
        serializeJson(info, payload);
        if (!mqttClient.publish(monitor_info_topic, payload.c_str(), true)) {
            LOG_ERROR("MQTT monitor: INFO publish failed");
        }

        /* Step 2c: Send the attributes again at the next attribute check: the broker may have
         * lost its retained messages (e.g. restarted without persistence) */
        if (mqttMutex != NULL && xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
            attrDirty = attrDoc.size() > 0;
            xSemaphoreGive(mqttMutex);
        }

        /* Step 3: The app's RPC topic */
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
    if (alarmQueue == NULL) {
        alarmQueue = xQueueCreate(MQTT_ALARM_QUEUE_LEN, sizeof(mqtt_alarm_t));
    }
    return mqttMutex != NULL && alarmQueue != NULL;
}

/**
 * @brief Publishes the queued alarms, oldest first. Called only by the MQTT task.
 * @note An alarm leaves the queue only after the broker connection accepted it, so a failed
 *       publish or a lost connection keeps it for the next pass.
 */
static void publishQueuedAlarms() {
    if (alarmQueue == NULL) return;                  /* mqtt_init() could not allocate it */
    mqtt_alarm_t a;
    while (mqttClient.connected() && xQueuePeek(alarmQueue, &a, 0) == pdTRUE) {
        char topic[MONITOR_TOPIC_MAX_LEN + MQTT_ALARM_CODE_LEN];
        snprintf(topic, sizeof(topic), "%s/%s/%s/%s",
                 MONITOR_TOPIC_ROOT, monitor_device_id, MONITOR_TOPIC_ALARM, a.code);

        JsonDocument doc;
        doc["msg"]    = a.msg;
        doc["source"] = a.source;
        doc["level"]  = a.level;
        doc["active"] = a.active;    /* false: the problem is gone, waiting for acknowledgement */
        String payload;
        serializeJson(doc, payload);

        if (!mqttClient.publish(topic, payload.c_str(), true)) {
            LOG_ERROR_STR("MQTT alarm publish failed: %s", a.code);
            break;                                   /* Keep it queued, retry next pass */
        }
        xQueueReceive(alarmQueue, &a, 0);            /* Sent: now remove it */
    }
}

/**
 * @brief Publishes all attributes (retained) if one changed since the last publish.
 *        Called only by the MQTT task, every MQTT_ATTR_CHECK_PERIOD_MS.
 */
static void publishAttributes() {
    if (mqttMutex == NULL || !mqttClient.connected()) return;

    /* Step 1: Test the flag; only when it is raised, copy the attributes out under the mutex */
    String payload;
    if (xSemaphoreTake(mqttMutex, portMAX_DELAY) != pdTRUE) return;
    bool dirty = attrDirty;
    if (dirty) {
        serializeJson(attrDoc, payload);
        attrDirty = false;
    }
    xSemaphoreGive(mqttMutex);
    if (!dirty) return;

    /* Step 2: Publish outside the mutex, so callers of mqtt_set_attribute() never wait on the network */
    if (!mqttClient.publish(monitor_attr_topic, payload.c_str(), true)) {
        LOG_ERROR("MQTT monitor: ATTR publish failed, payload bytes=" + String(payload.length()));
        if (xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
            attrDirty = true;                        /* Retry at the next check */
            xSemaphoreGive(mqttMutex);
        }
    }
}

/**
 * @brief MQTT task: connect to the broker, collect telemetry and publish periodically.
 * @param[in] pvParameters Unused.
 */
void vMqttTask(void *pvParameters) {
    core_state_wait(g_wifi_state_group, (uint8_t)WIFI_STATE_STA, portMAX_DELAY);
    
    loadMqttConfig();

    /* Central monitor topics: DEVICE_ID = Wi-Fi station MAC as 12 hex digits, no ':' */
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(monitor_device_id, sizeof(monitor_device_id), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    snprintf(monitor_status_topic, sizeof(monitor_status_topic), "%s/%s/%s",
             MONITOR_TOPIC_ROOT, monitor_device_id, MONITOR_TOPIC_STATUS);
    snprintf(monitor_info_topic, sizeof(monitor_info_topic), "%s/%s/%s",
             MONITOR_TOPIC_ROOT, monitor_device_id, MONITOR_TOPIC_INFO);
    snprintf(monitor_data_topic, sizeof(monitor_data_topic), "%s/%s/%s",
             MONITOR_TOPIC_ROOT, monitor_device_id, MONITOR_TOPIC_DATA);
    snprintf(monitor_attr_topic, sizeof(monitor_attr_topic), "%s/%s/%s",
             MONITOR_TOPIC_ROOT, monitor_device_id, MONITOR_TOPIC_ATTR);

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

    /* loop() runs every MQTT_LOOP_PERIOD_MS to keep the connection alive (keepalive pings,
     * incoming RPC); telemetry is only published every mqtt_interval */
    TickType_t lastPublish = xTaskGetTickCount();
    TickType_t lastAttrCheck = xTaskGetTickCount();
    TickType_t lastConnectTry = 0;
    bool firstConnectTry = true;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(MQTT_LOOP_PERIOD_MS));

        if (is_wifi_connected()) {
            if (!mqttClient.connected()) {
                /* connect() blocks: don't retry on every pass while the broker is unreachable */
                TickType_t now = xTaskGetTickCount();
                if (firstConnectTry || now - lastConnectTry >= pdMS_TO_TICKS(MQTT_RECONNECT_PERIOD_MS)) {
                    firstConnectTry = false;
                    lastConnectTry = now;
                    connectBroker();
                }
            }

            mqttClient.loop();

            /* Alarms go out at once (every pass), not on the telemetry interval */
            publishQueuedAlarms();

            /* Attributes: test the "changed" flag about once per second */
            if (xTaskGetTickCount() - lastAttrCheck >= pdMS_TO_TICKS(MQTT_ATTR_CHECK_PERIOD_MS)) {
                lastAttrCheck = xTaskGetTickCount();
                publishAttributes();
            }

            if (mqttClient.connected() &&
                xTaskGetTickCount() - lastPublish >= pdMS_TO_TICKS(mqtt_interval)) {
                lastPublish = xTaskGetTickCount();

                /* Step 0: clientID and ip are attributes: stored here, sent only when they change.
                 * Called before taking the mutex: mqtt_set_attribute() takes it itself. */
                String clientID = getWifiClientID();
                mqtt_set_attribute("clientID", clientID.length() > 0 ? clientID : (String(esp_info_get_model()) + "_" + String(esp_info_get_mac_str())));
                mqtt_set_attribute("ip", WiFi.localIP().toString());

                /* Step 1: Refresh system metadata under the mutex. The document is NOT cleared:
                 * script/driver values pushed via mqtt_add_telemetry() between publishes must
                 * survive until the next publish; each key is simply overwritten on update. */
                if (mqttMutex != NULL && xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
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
                    /* Same payload for the central monitor, not retained */
                    if (!mqttClient.publish(monitor_data_topic, jsonBuffer.c_str())) {
                        LOG_ERROR("MQTT monitor: DATA publish failed, payload bytes=" + String(jsonBuffer.length()));
                    }
                    xSemaphoreGive(mqttMutex);
                }
            }
        }
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

/* Same value already stored? Numbers and bools compare as their type, text compares by content. */
template <typename T>
static bool attr_same(JsonVariantConst cur, T value) {
    return cur.is<T>() && cur.as<T>() == value;
}
static bool attr_same(JsonVariantConst cur, const char *value) {
    const char *s = cur.as<const char *>();
    return cur.is<const char *>() && s != NULL && strcmp(s, value ? value : "") == 0;
}
static bool attr_same(JsonVariantConst cur, char *value) {
    return attr_same(cur, (const char *)value);
}
static bool attr_same(JsonVariantConst cur, const String &value) {
    return attr_same(cur, value.c_str());
}

template <typename T>
bool mqtt_set_attribute(const String &key, T value) {
    /* Step 1: Reject the call if mqtt_init() has not created the mutex yet */
    if (mqttMutex == NULL || key.length() == 0) return false;

    if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(MQTT_TELEMETRY_LOCK_TIMEOUT_MS)) != pdTRUE) {
        LOG_ERROR("mqtt_set_attribute: mutex timeout for key " + key);
        return false;
    }
    /* Step 2: Store and raise the flag only when the value really changed; the MQTT task
     * publishes all attributes at its next check */
    JsonVariantConst cur = attrDoc[key];
    if (!attr_same(cur, value)) {
        attrDoc[key] = value;
        attrDirty = true;
    }
    xSemaphoreGive(mqttMutex);
    return true;
}

template bool mqtt_set_attribute<int>(const String&, int);
template bool mqtt_set_attribute<float>(const String&, float);
template bool mqtt_set_attribute<double>(const String&, double);
template bool mqtt_set_attribute<char*>(const String&, char*);
template bool mqtt_set_attribute<const char*>(const String&, const char*);
template bool mqtt_set_attribute<String>(const String&, String);
template bool mqtt_set_attribute<bool>(const String&, bool);

bool is_mqtt_connected() {
    return mqttClient.connected();
}

bool mqtt_alarm_publish(const char *code, const char *msg, const char *source, const char *level, bool active) {
    /* The code is the last topic level: not empty, fits, and no MQTT separator or wildcard */
    if (alarmQueue == NULL || code == NULL || code[0] == '\0' ||
        strlen(code) >= MQTT_ALARM_CODE_LEN || strpbrk(code, "/+# ") != NULL) {
        LOG_ERROR_STR("MQTT alarm rejected: invalid code '%s'", code ? code : "");
        return false;
    }

    mqtt_alarm_t a;
    strncpy(a.code, code, sizeof(a.code) - 1);
    a.code[sizeof(a.code) - 1] = '\0';
    strncpy(a.msg, msg ? msg : "", sizeof(a.msg) - 1);          /* Longer text is cut */
    a.msg[sizeof(a.msg) - 1] = '\0';
    strncpy(a.source, source ? source : "", sizeof(a.source) - 1);
    a.source[sizeof(a.source) - 1] = '\0';
    strncpy(a.level, level ? level : "", sizeof(a.level) - 1);    /* Any text the caller chooses */
    a.level[sizeof(a.level) - 1] = '\0';
    a.active = active;

    /* Never block the caller: a full queue (broker unreachable for long) rejects the alarm */
    if (xQueueSend(alarmQueue, &a, 0) != pdTRUE) {
        LOG_WARNING_STR("MQTT alarm queue full, dropped: %s", a.code);
        return false;
    }
    return true;
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

#else //USE_MQTT

/* -------------------------------------------------------------------------- */
/*              MQTT DISABLED: default returns, no task, no commands          */
/* -------------------------------------------------------------------------- */

String getMqttServer() { return String(); }
uint16_t getMqttPort() { return 0; }
String getMqttUser() { return String(); }
String getMqttPass() { return String(); }
uint32_t getMqttInterval() { return 0; }
String getMqttDataTopic() { return String(); }
String getMqttRpcTopic() { return String(); }

void updateMqttConfig(const String &server, uint16_t port, const String &user,
                      const String &pass, uint32_t interval, const String &dataTopic,
                      const String &rpcTopic) {}

bool mqtt_init() { return false; }

template <typename T>
bool mqtt_add_telemetry(const String &key, T value) { return false; }

template bool mqtt_add_telemetry<int>(const String&, int);
template bool mqtt_add_telemetry<float>(const String&, float);
template bool mqtt_add_telemetry<double>(const String&, double);
template bool mqtt_add_telemetry<char*>(const String&, char*);
template bool mqtt_add_telemetry<const char*>(const String&, const char*);
template bool mqtt_add_telemetry<String>(const String&, String);
template bool mqtt_add_telemetry<bool>(const String&, bool);

template <typename T>
bool mqtt_set_attribute(const String &key, T value) { return false; }

template bool mqtt_set_attribute<int>(const String&, int);
template bool mqtt_set_attribute<float>(const String&, float);
template bool mqtt_set_attribute<double>(const String&, double);
template bool mqtt_set_attribute<char*>(const String&, char*);
template bool mqtt_set_attribute<const char*>(const String&, const char*);
template bool mqtt_set_attribute<String>(const String&, String);
template bool mqtt_set_attribute<bool>(const String&, bool);

bool is_mqtt_connected() { return false; }

bool mqtt_alarm_publish(const char *code, const char *msg, const char *source, const char *level, bool active) { return false; }

void mqtt_task_start() {}

void vMqttTask(void *pvParameters) { vTaskDelete(NULL); }

#endif //USE_MQTT
