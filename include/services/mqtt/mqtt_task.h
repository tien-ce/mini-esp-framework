#ifndef MQTT_TASK_H
#define MQTT_TASK_H

#include <Arduino.h>
#include <ArduinoJson.h>

/* -------------------------------------------------------------------------- */
/*                              PUBLIC CONFIG APIS                            */
/* -------------------------------------------------------------------------- */

String getMqttServer();
uint16_t getMqttPort();
String getMqttUser();
String getMqttPass();
uint32_t getMqttInterval();
String getMqttDataTopic();
String getMqttRpcTopic();

void updateMqttConfig(const String &server, uint16_t port, const String &user,
                      const String &pass, uint32_t interval, const String &dataTopic,
                      const String &rpcTopic);

/* -------------------------------------------------------------------------- */
/*                              PUBLIC DATA API                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief Thread-safe API for drivers to add any telemetry key-value pair.
 */
template <typename T>
void mqtt_add_telemetry(const String &key, T value);

/* -------------------------------------------------------------------------- */
/*                              CORE ENGINE API                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief FreeRTOS task quản lý kết nối và xuất bản telemetry lên MQTT Broker định kỳ.
 * 
 * @details Task chờ sự kiện mạng sẵn sàng (NET_STATE_WIFI_STA), tải cấu hình từ NVS, duy trì kết nối tới Broker
 *          qua hàm loop(), và chu kỳ thu thập telemetry/xuất bản dữ liệu.
 *          Để phòng tránh tình trạng Deadlock giữa các luồng khi các driver phần cứng xử lý tín hiệu SIG_MQTT_PUBLISH,
 *          task áp dụng cơ chế giải phóng Mutex trước khi dispatch tín hiệu và chỉ chiếm lại Mutex khi serialize JSON.
 * 
 * @param[in] pvParameters Con trỏ tham số truyền vào từ FreeRTOS xTaskCreate (không sử dụng, có thể là NULL).
 */
void vMqttTask(void *pvParameters);

#endif // MQTT_TASK_H
