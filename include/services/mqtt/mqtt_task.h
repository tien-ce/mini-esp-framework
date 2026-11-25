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

/**
 * @brief Thread-safe API to set an attribute: a value that rarely changes (serial, setpoint,
 *        clientID, ip...), published retained on IOT-MONITOR/<DEVICE_ID>/ATTR instead of with
 *        every telemetry payload.
 *
 * Never waits on the network: it only stores the value. When the value differs from the stored
 * one it raises a "changed" flag; the MQTT task tests that flag about once per second and then
 * publishes ALL attributes in one message (also again after every reconnect).
 * Supported value types: int, float, double, bool, const char*, char*, String.
 * Must not be called while the caller already holds the MQTT mutex.
 *
 * @param[in] key   Attribute name, not empty.
 * @param[in] value Attribute value.
 * @return true if stored (changed or not), false if the mutex could not be taken, the key is
 *         empty, or the build has no MQTT.
 */
template <typename T>
bool mqtt_set_attribute(const String &key, T value);

/**
 * @brief Sends an alarm to the central monitor: IOT-MONITOR/<DEVICE_ID>/ALARM/<code> (retained).
 *
 * Safe from any task and never blocks: it only queues a copy, the MQTT task publishes it at once
 * when connected, or later after a reconnect. The alarm stays on the monitor until someone
 * acknowledges it there.
 *
 * @param[in] code   Alarm code, the last topic level, e.g. "MODBUS_READ". Not empty, at most 23
 *                   characters, no '/', '+', '#' or space.
 * @param[in] msg    Text for people (cut at 95 characters). NULL = "".
 * @param[in] source Who raised it, e.g. a script or driver name (cut at 23 characters). NULL = "".
 * @param[in] level  Severity as free text chosen by the caller, e.g. "warning", "critical"
 *                   (cut at 15 characters). NULL = "".
 * @param[in] active true: the problem is happening. false: it is gone, waiting for acknowledgement.
 * @return true (OK) when queued; false (ERROR) for an invalid code, a full queue, or a build
 *         without MQTT.
 */
bool mqtt_alarm_publish(const char *code, const char *msg, const char *source, const char *level, bool active);

/* -------------------------------------------------------------------------- */
/*                              CORE ENGINE API                               */
/* -------------------------------------------------------------------------- */

bool is_mqtt_connected();

/**
 * @brief Starts the FreeRTOS MQTT client task.
 */
void mqtt_task_start();

/**
 * @brief FreeRTOS task that maintains the MQTT broker connection and periodically publishes telemetry.
 *
 * @param[in] pvParameters FreeRTOS task parameter (unused, may be NULL).
 */
void vMqttTask(void *pvParameters);

#endif // MQTT_TASK_H
