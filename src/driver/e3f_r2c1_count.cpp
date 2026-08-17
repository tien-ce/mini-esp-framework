#include "config.h"
#ifdef USE_E3FR2C1_COUNT
#include "core/wifi_task.h"
#include "core/core_engine.h"
#include "core/pin_config.h"
#include <HTTPClient.h>
#include "Header_sensor.h"
#include <esp_task_wdt.h>
#include <semphr.h>

/* -------------------------------------------------------------------------- */
/*                              GLOBAL VARIABLES                              */
/* -------------------------------------------------------------------------- */

// FreeRTOS Inter-Task Queue Handle
QueueHandle_t sensorQueue = NULL;

/* -------------------------------------------------------------------------- */
/*                              STATIC VARIABLES                              */
/* -------------------------------------------------------------------------- */

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
static uint8_t html_id = 0;

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Resets sensor count to zero in a thread-safe manner.
 */
static void resetSensorCount() {
    if (countMutex != NULL && xSemaphoreTake(countMutex, portMAX_DELAY) == pdTRUE) {
        cnt = 0;
        xSemaphoreGive(countMutex);
    }
}

// Read operation: No mutex used
static String getSensorClientID() {
    return sensor_client_id;
}

// Read operation: No mutex used
static String getSensorApiUrl() {
    return sensor_api_url;
}

/**
 * @brief Returns current sensor count value.
 */
static int getSensorCount() {
    return cnt;
}

/**
 * @brief Increments sensor count in a thread-safe manner.
 */
static void incrementSensorCount() {
    if (countMutex != NULL && xSemaphoreTake(countMutex, portMAX_DELAY) == pdTRUE) {
        cnt++;
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

/**
 * @brief Saves sensor driver configuration parameters to NVS.
 */
static void saveSensorConfig() {
    initSensorMutex();
    if (configMutex != NULL && xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        if (config_get_lock()) {
            config_save_string("sensor_driver", "clientId", sensor_client_id);
            config_save_string("sensor_driver", "apiUrl", sensor_api_url);
            config_release_lock();
        }
        xSemaphoreGive(configMutex);
    }
}

/**
 * @brief Loads sensor driver configuration parameters from NVS.
 */
static void loadSensorConfig() {
    initSensorMutex();
    register_config_module("sensor_driver", "sensor_drv");

    if (configMutex != NULL && xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        if (config_get_lock()) {
            sensor_client_id = config_read_string("sensor_driver", "clientId", E3FR2C1_COUNT_CLIENT_ID);
            sensor_api_url = config_read_string("sensor_driver", "apiUrl", E3FR2C1_COUNT_API_URL);
            config_release_lock();
        }
        xSemaphoreGive(configMutex);
    }
    LOG_INFO("Sensor config loaded from NVS successfully.");
}

/**
 * @brief Task 1: Sensor Sampling & Edge Detection Task
 */
static void vSensorTask(void *pvParameters) {
    // Check if the pin name is assigned
    if (!is_use_name("E3FR2C1_IN")) {
        LOG_INFO("E3FR2C1_IN pin is not configured. Aborting task.");
        vTaskDelete(NULL);
        return;
    }

    // Resolve GPIO number associated with "E3FR2C1_IN"
    int8_t e3f_pin = GPIO_INVALID;
    for (int i = 0; i < MAX_GPIO_PINS; i++) {
        if (get_pin_name(i).equalsIgnoreCase("E3FR2C1_IN")) {
            e3f_pin = i;
            break;
        }
    }

    if (e3f_pin == GPIO_INVALID) {
        LOG_ERROR("Failed to resolve GPIO for E3FR2C1_IN.");
        vTaskDelete(NULL);
        return;
    }

    LOG_INFO("E3FR2C1_IN initialized on GPIO " + String(e3f_pin));
    pinMode(e3f_pin, INPUT);

    bool senHigh = false;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10); // Poll every 10 ms
    
    html_id = registerElement("Sensor count", "", "0");
    LOG_DEBUG("Registered sensor element with ID: " + String(html_id));

    for (;;) {
        int sensorOutput = digitalRead(e3f_pin);

        if (sensorOutput == HIGH) {
            if (!senHigh) {
                // Rising edge detected (LOW -> HIGH)
                senHigh = true;

                incrementSensorCount();
                int currentCount = getSensorCount();

                LOG_DEBUG("Sensor edge detected on GPIO " + String(e3f_pin) + " | Count: " + String(currentCount));
                updateElementValue(html_id, String(currentCount));

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
static void vNetworkTask(void *pvParameters) {
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

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

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
#endif // USE_E3FR2C1_COUNT

