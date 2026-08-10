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
 * @brief Entry point invoked exclusively by core_engine to start MQTT Task.
 */
void vMqttTask(void *pvParameters);

#endif // MQTT_TASK_H
