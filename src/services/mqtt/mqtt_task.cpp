#include "services/mqtt/mqtt_task.h"
#include "core/core_nvs.h"
#include "core/core_engine.h"
#include "core/dispatcher.h"
#include "core/core_log.h"
#include "core/info.h"
#include "services/wifi/wifi_task.h"
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
    initMqttMutex();
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


/**
 * @brief FreeRTOS task quản lý vòng đời kết nối MQTT Broker và chu kỳ xuất bản dữ liệu Telemetry.
 * @param[in] pvParameters Tham số tác vụ FreeRTOS (không sử dụng).
 */
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
                /*
                 * GIAI ĐOẠN 1: Nạp metadata hệ thống vào đối tượng JSON (Vùng găng 1)
                 * Lấy mqttMutex để đảm bảo an toàn truy cập vào tài nguyên dùng chung mqttDoc.
                 * Ghi nhận thông tin clientID, IP, RSSI, heap trống và uptime.
                 */
                if (mqttMutex != NULL && xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
                    mqttDoc.clear();

                    String clientID = getWifiClientID();
                    mqttDoc["clientID"] = clientID.length() > 0 ? clientID : (String(esp_info_get_model()) + "_" + String(esp_info_get_mac_str()));
                    mqttDoc["ip"]       = WiFi.localIP().toString();
                    mqttDoc["rssi"]     = WiFi.RSSI();
                    mqttDoc["freeHeap"] = ESP.getFreeHeap();
                    mqttDoc["uptime"]   = millis() / 1000;

                    /*
                     * CƠ CHẾ NHẢ MUTEX CHỐNG DEADLOCK (Deadlock Prevention Mechanism):
                     * BẮT BUỘC nhả mqttMutex tại đây trước khi phát tín hiệu SIG_MQTT_PUBLISH vì các lý do cốt lõi sau:
                     * 1. Phòng chống Deadlock vòng tròn (Circular Wait):
                     *    Khi gọi dispatch_signal(SIG_MQTT_PUBLISH), Core Engine sẽ gọi trực tiếp và đồng bộ callback
                     *    của tất cả các driver phần cứng đã đăng ký. Trong quá trình xử lý, nếu một driver gọi đến bất kỳ
                     *    hàm nào cần lấy mqttMutex (ví dụ: cập nhật cấu hình MQTT, hoặc đồng bộ hóa với một task khác đang
                     *    chờ mqttMutex), việc tiếp tục giữ Mutex sẽ gây ra tình trạng khóa chết hệ thống (Deadlock).
                     * 2. Thu hẹp vùng găng (Minimizing Critical Section):
                     *    Các thao tác đọc cảm biến của driver phần cứng (như đo đạc I2C, SPI, OneWire) có thể tiêu tốn
                     *    thời gian trễ (latency). Giải phóng Mutex giúp các tác vụ khác (như Web Server Task gọi
                     *    updateMqttConfig) không bị nghẽn (blocking) trong suốt khoảng thời gian phần cứng thực thi.
                     */
                    xSemaphoreGive(mqttMutex);
                }

                // Core engine dispatches signal for driver data population
                // Các driver sẽ nhận tín hiệu và gọi mqtt_add_telemetry() để bổ sung dữ liệu đo đạc vào payload
                dispatch_signal(SIG_MQTT_PUBLISH);

                /*
                 * GIAI ĐOẠN 2: Chiếm lại Mutex để Serialize JSON và Publish (Vùng găng 2)
                 * Sau khi toàn bộ driver đã hoàn tất việc nạp dữ liệu telemetry vào mqttDoc,
                 * task mới chiếm lại mqttMutex để serialize chuỗi JSON và gửi dữ liệu lên Broker một cách toàn vẹn.
                 */
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
