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
 * @brief Create the MQTT mutex. Call once, single-threaded, before starting any task that uses the MQTT API.
 *
 * @return true if the mutex exists after the call, false on allocation failure.
 */
bool mqtt_init();

/**
 * @brief Thread-safe API to add a telemetry key-value pair to the MQTT payload.
 *
 * Supported value types: int, float, double, bool, const char*, char*, String.
 * Must not be called while the caller already holds the MQTT mutex.
 *
 * @param[in] key   Telemetry field name.
 * @param[in] value Telemetry field value.
 * @return true if the pair was stored, false if the mutex could not be taken.
 */
template <typename T>
bool mqtt_add_telemetry(const String &key, T value);

/* -------------------------------------------------------------------------- */
/*                              CORE ENGINE API                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief FreeRTOS task that maintains the MQTT broker connection and periodically publishes telemetry.
 *
 * @param[in] pvParameters FreeRTOS task parameter (unused, may be NULL).
 */
void vMqttTask(void *pvParameters);

#endif // MQTT_TASK_H
