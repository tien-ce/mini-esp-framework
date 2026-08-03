#include "core/wifi_task.h"
#include "core/core_engine.h"
#include "config.h"
#include <HTTPClient.h>
#include "Header_sensor.h"
#include <esp_task_wdt.h>
#include <semphr.h>

// FreeRTOS Inter-Task Queue Handle
QueueHandle_t sensorQueue = NULL;

// HTTP Cooldown Parameters
static unsigned long lastHttpFail = 0;
static const unsigned long httpCooldown = 5000; // 5-second cooldown on failure
static int httpFailCount = 0;

// Runtime state variables & mutexes
static int cnt = 0;
static SemaphoreHandle_t countMutex = NULL;
static SemaphoreHandle_t configMutex = NULL;

// Sensor Driver Configuration
static String sensor_client_id = "";
static String sensor_api_url = "";

// Read operation: No mutex used
static int getSensorCount() {
    return cnt;
}

// Write operation: Protected by mutex
static void incrementSensorCount() {
    if (countMutex != NULL && xSemaphoreTake(countMutex, portMAX_DELAY) == pdTRUE) {
        cnt++;
        xSemaphoreGive(countMutex);
    }
}

// Write operation: Protected by mutex
void resetSensorCount() {
    if (countMutex != NULL && xSemaphoreTake(countMutex, portMAX_DELAY) == pdTRUE) {
        cnt = 0;
        xSemaphoreGive(countMutex);
    }
}

/**
 * @brief Initializes the FreeRTOS mutexes for config and counter protection.
 */
static void initSensorMutex() {
    if (countMutex == NULL) {
        countMutex = xSemaphoreCreateMutex();
    }
    if (configMutex == NULL) {
        configMutex = xSemaphoreCreateMutex();
    }
}

void saveSensorConfig() {
    initSensorMutex();
    String content = "";
    // Read local variables to build payload, no lock required for simple string concatenation copy
    content += "clientId=" + sensor_client_id + "\n";
    content += "apiUrl=" + sensor_api_url + "\n";    
    save_config("sensor_driver", content);
}

void loadSensorConfig() {
    initSensorMutex();
    register_config_file("sensor_driver", "sensor_config.txt");

    String raw = read_config("sensor_driver");
    
    if (raw.length() == 0) {
        LOG_INFO("Sensor config file not found or empty. Creating default sensor_config.txt");
        if (configMutex != NULL && xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
            sensor_client_id = ANDON_CLIENT_ID;
            sensor_api_url = HTTP_URL;
            xSemaphoreGive(configMutex);
        }
        saveSensorConfig();
        return;
    }

    String tempClientId = "";
    String tempApiUrl = "";

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
                tempClientId = val;
            } else if (key.equalsIgnoreCase("apiUrl")) {
                tempApiUrl = val;
            }
        }
    }

    // Write parsed configuration under mutex protection
    if (configMutex != NULL && xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        sensor_client_id = tempClientId;
        sensor_api_url = tempApiUrl;
        xSemaphoreGive(configMutex);
    }

    LOG_INFO("Sensor config loaded successfully.");
}

// Read operation: No mutex used
String getSensorClientID() {
    return sensor_client_id;
}

// Read operation: No mutex used
String getSensorApiUrl() {
    return sensor_api_url;
}

// Write operation: Protected by mutex
void updateSensorConfig(const String &newClientID, const String &newApiUrl) {
    initSensorMutex();
    if (configMutex != NULL && xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        sensor_client_id = newClientID;
        sensor_api_url = newApiUrl;
        xSemaphoreGive(configMutex);
    }
    saveSensorConfig();
}

/**
 * @brief Task 1: Sensor Sampling & Edge Detection Task
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

    // Task 2: Network Task (Priority 2 - Medium)
    //xTaskCreatePinnedToCore(
    //    vNetworkTask,
    //    "NetworkTask",
    //    4096,
    //    NULL,
    //    2,
    //    NULL,
    //    1
    //);
}
