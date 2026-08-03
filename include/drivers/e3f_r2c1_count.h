#ifndef E3F_R2C1_COUNT_H
#define E3F_R2C1_COUNT_H

#include <Arduino.h>
#include <FreeRTOS.h>
#include <queue.h>
#include "Header_sensor.h"
#include "core/config_manager.h"
#include "core/log_task.h"

// FreeRTOS Inter-Task Queue Handle for Sensor Events
extern QueueHandle_t sensorQueue;

/**
 * @brief Loads sensor driver configuration (clientID, api_url) from LittleFS sensor_config.txt.
 * @param None
 * @return None
 */
void loadSensorConfig();

/**
 * @brief Saves current sensor driver configuration to LittleFS sensor_config.txt.
 * @param None
 * @return None
 */
void saveSensorConfig();

/**
 * @brief Gets current sensor client ID in a thread-safe manner.
 * @param None
 * @return String containing client ID.
 */
String getSensorClientID();

/**
 * @brief Gets current sensor API URL target in a thread-safe manner.
 * @param None
 * @return String containing API URL.
 */
String getSensorApiUrl();

/**
 * @brief Updates sensor driver configuration parameters and saves to LittleFS.
 * @param newClientID New client ID string.
 * @param newApiUrl New API URL string.
 * @return None
 */
void updateSensorConfig(const String &newClientID, const String &newApiUrl);

/**
 * @brief High-frequency digital pin polling task for detecting sensor edge transitions.
 * @param pvParameters Pointer to FreeRTOS task parameters.
 * @return None
 */
void vSensorTask(void *pvParameters);

/**
 * @brief Network transmission task for processing sensor events and executing HTTP GET requests.
 * @param pvParameters Pointer to FreeRTOS task parameters.
 * @return None
 */
void vNetworkTask(void *pvParameters);

/**
 * @brief Initializes application sensor queue and spawns FreeRTOS sensor sampling task.
 * @param None
 * @return None
 */
void initSensorTasks();

#endif // E3F_R2C1_COUNT_H



