#include "drivers/e3f_r2c1_count.h"
#include "core/wifi_task.h"
#include "core/core_engine.h"
#include <HTTPClient.h>
#include <esp_task_wdt.h>
#include <semphr.h>

// FreeRTOS Inter-Task Queue Handle
QueueHandle_t sensorQueue = NULL;

// HTTP Cooldown Parameters
static unsigned long lastHttpFail = 0;
static const unsigned long httpCooldown = 5000; // 5-second cooldown on failure
static int httpFailCount = 0;

// Sensor Driver Configuration
static String sensor_client_id = "ANDONAD";
static String sensor_api_url   = "http://192.168.1.13/test/arduino/sensorcount?name=";
static SemaphoreHandle_t sensorConfigMutex = NULL;

/**
 * @brief Initializes the FreeRTOS mutex for protecting sensor driver configuration.
 * @param None
 * @return None
 */
static void initSensorMutex() {
    if (sensorConfigMutex == NULL) {
        sensorConfigMutex = xSemaphoreCreateMutex();
    }
}

void loadSensorConfig() {
    initSensorMutex();
    register_config_file("sensor_driver", "sensor_config.txt");

    String raw = read_config("sensor_driver");
    if (raw.length() == 0) {
        LOG_INFO("Sensor config file not found or empty. Creating default sensor_config.txt");
        saveSensorConfig();
        return;
    }

    if (sensorConfigMutex != NULL && xSemaphoreTake(sensorConfigMutex, portMAX_DELAY) == pdTRUE) {
        int pos = 0;
        while (pos < raw.length()) {
            int nextPos = raw.indexOf('\n', pos);
            if (nextPos == -1) nextPos = raw.length();
            String line = raw.substring(pos, nextPos);
            line.trim();
            pos = nextPos + 1;

            if (line.length() == 0) continue;
            int eqIdx = line.indexOf('=');
            if (eqIdx > 0) {
                String key = line.substring(0, eqIdx);
                String val = line.substring(eqIdx + 1);
                key.trim();
                val.trim();

                if (key.equalsIgnoreCase("clientId")) {
                    sensor_client_id = val;
                } else if (key.equalsIgnoreCase("apiUrl")) {
                    sensor_api_url = val;
                }
            }
        }
        xSemaphoreGive(sensorConfigMutex);
    }
    LOG_INFO("Sensor config loaded successfully.");
}

void saveSensorConfig() {
    initSensorMutex();
    String content = "";
    if (sensorConfigMutex != NULL && xSemaphoreTake(sensorConfigMutex, portMAX_DELAY) == pdTRUE) {
        content += "clientId=" + sensor_client_id + "\n";
        content += "apiUrl=" + sensor_api_url + "\n";
        xSemaphoreGive(sensorConfigMutex);
    }
    save_config("sensor_driver", content);
}

String getSensorClientID() {
    initSensorMutex();
    String val = "";
    if (sensorConfigMutex != NULL && xSemaphoreTake(sensorConfigMutex, portMAX_DELAY) == pdTRUE) {
        val = sensor_client_id;
        xSemaphoreGive(sensorConfigMutex);
    }
    return val;
}

String getSensorApiUrl() {
    initSensorMutex();
    String val = "";
    if (sensorConfigMutex != NULL && xSemaphoreTake(sensorConfigMutex, portMAX_DELAY) == pdTRUE) {
        val = sensor_api_url;
        xSemaphoreGive(sensorConfigMutex);
    }
    return val;
}

void updateSensorConfig(const String &newClientID, const String &newApiUrl) {
    initSensorMutex();
    if (sensorConfigMutex != NULL && xSemaphoreTake(sensorConfigMutex, portMAX_DELAY) == pdTRUE) {
        sensor_client_id = newClientID;
        sensor_api_url = newApiUrl;
        xSemaphoreGive(sensorConfigMutex);
    }
    saveSensorConfig();
}

/**
 * @brief Task 1: Sensor Sampling & Edge Detection Task
 * @priority 3 (High Priority)
 * @core Core 1
 * 
 * High-frequency digital pin polling task (runs every 10ms). Detects rising edge
 * transition (LOW -> HIGH) on IN_1 (GPIO 47). Safely increments counter 'cnt'
 * using countMutex and pushes count event into 'sensorQueue' for async network logging.
 * @param pvParameters Pointer to FreeRTOS task parameters.
 * @return None
 */
void vSensorTask(void *pvParameters) {
    bool senHigh = false;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10); // Poll every 10 ms

    for (;;) {
        int sensorOutput = digitalRead(IN_1);

        if (sensorOutput == HIGH) {
            if (!senHigh) {
                // Rising edge detected (LOW -> HIGH)
                senHigh = true;

                incrementSensorCount();
                int currentCount = getSensorCount();

                LOG("Sensor edge detected | Count: " + String(currentCount));

                // Post event to Queue for Network Task (non-blocking if queue full)
                if (sensorQueue != NULL) {
                    xQueueSend(sensorQueue, &currentCount, 0);
                }
            }
        } else {
            // Signal returned LOW, reset edge flag
            senHigh = false;
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief Task 2: Network Transmission & HTTP Logging Task
 * @priority 2 (Medium Priority)
 * @core Core 1
 * 
 * Process count events from sensorQueue. Executes HTTP GET request to external API.
 * Includes timeout (3s) and failure cooldown (5s) management to prevent network blocking.
 * @param pvParameters Pointer to FreeRTOS task parameters.
 * @return None
 */
void vNetworkTask(void *pvParameters) {
    int countVal = 0;
    HTTPClient http;

    for (;;) {
        // Wait for sensor trigger event from Queue (timeout after 1000ms)
        if (sensorQueue != NULL && xQueueReceive(sensorQueue, &countVal, pdMS_TO_TICKS(1000)) == pdPASS) {
            if (is_wifi_connected()) {
                unsigned long currentMillis = millis();

                // Check HTTP cooldown period
                if (currentMillis - lastHttpFail < httpCooldown && httpFailCount > 0) {
                    LOG_WARNING("HTTP cooldown active... skipping request | Count: " + String(countVal));
                } else {
                    String fullUrl = getSensorApiUrl() + getSensorClientID();

                    if (fullUrl.length() > 0) {
                        http.begin(fullUrl);
                        http.setTimeout(3000); // 3-second timeout

                        int httpCode = http.GET();

                        if (httpCode == HTTP_CODE_OK) {
                            String payload = http.getString();
                            LOG_INFO("HTTP OK: " + payload + " | Count: " + String(countVal));
                            httpFailCount = 0;
                        } else if (httpCode > 0) {
                            LOG_WARNING("HTTP Error Code: " + String(httpCode) + " | Count: " + String(countVal));
                            httpFailCount++;
                            lastHttpFail = currentMillis;
                        } else {
                            LOG_ERROR("HTTP Failed: " + http.errorToString(httpCode));
                            httpFailCount++;
                            lastHttpFail = currentMillis;
                        }
                        http.end();
                    }
                }
            } else {
                LOG_WARNING("WiFi disconnected. Skipping HTTP send for Count: " + String(countVal));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/**
 * @brief Initialization function for Application Sensor Tasks.
 * @param None
 * @return None
 */
void initSensorTasks() {
    loadSensorConfig();
    sensorQueue = xQueueCreate(20, sizeof(int));

    // Task 1: Sensor Sampling Task (Priority 3 - High)
    xTaskCreatePinnedToCore(
        vSensorTask,
        "SensorTask",
        3072,
        NULL,
        3,
        NULL,
        1
    );
}


