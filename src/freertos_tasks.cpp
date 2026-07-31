#include "freertos_tasks.h"
#include "log_task.h"
#include "wifi_task.h"
#include <HTTPClient.h>
#include <esp_task_wdt.h>

// FreeRTOS Inter-Task Queue Handle
QueueHandle_t sensorQueue = NULL;

// HTTP Cooldown Parameters
static unsigned long lastHttpFail = 0;
static const unsigned long httpCooldown = 5000; // 5-second cooldown on failure
static int httpFailCount = 0;

static unsigned int string_hash(const String str) {
    return string_hash(str.c_str());
}

constexpr unsigned int string_hash(const char *str) {
    unsigned int hash = 5381;
    while (*str != '\0') {
        hash = ((hash << 5) + hash) + static_cast<unsigned char>(*str);
        str++;
    }
    return hash;
}

/**
 * @brief Task 1: Sensor Sampling & Edge Detection Task
 * @priority 3 (High Priority)
 * @core Core 1
 * 
 * High-frequency digital pin polling task (runs every 10ms). Detects rising edge
 * transition (LOW -> HIGH) on IN_1 (GPIO 47). Safely increments counter 'cnt'
 * using countMutex and pushes count event into 'sensorQueue' for async network logging.
 */
static void vSensorTask(void *pvParameters) {
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
 */
static void vNetworkTask(void *pvParameters) {
    int countVal = 0;
    HTTPClient http;

    for (;;) {
        // Wait for sensor trigger event from Queue (timeout after 1000ms)
        if (sensorQueue != NULL && xQueueReceive(sensorQueue, &countVal, pdMS_TO_TICKS(1000)) == pdPASS) {
            if (WiFi.status() == WL_CONNECTED) {
                unsigned long currentMillis = millis();

                // Check HTTP cooldown period
                if (currentMillis - lastHttpFail < httpCooldown && httpFailCount > 0) {
                    LOG_WARNING("HTTP cooldown active... skipping request | Count: " + String(countVal));
                } else {
                    String fullUrl = getApiUrl() + getClientID();

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
 * @brief Task 3: Web Server & System Maintenance Task
 * @priority 1 (Low Priority)
 * @core Core 1
 * 
 * Periodically cleans up inactive WebSocket clients and performs background maintenance.
 */
static void vWebMonitorTask(void *pvParameters) {
    for (;;) {
        ws.cleanupClients();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* External API*/
void initFreeRTOSTasks() {
    sensorQueue = xQueueCreate(20, sizeof(int));

    // Create FreeRTOS Tasks pinned to Core 1
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

    // Task 2: Network & HTTP Logging Task (Priority 2 - Medium)
    // xTaskCreatePinnedToCore(
    //     vNetworkTask,
    //     "NetworkTask",
    //     8192,
    //     NULL,
    //     2,
    //     NULL,
    //     1
    // );

    // Task 3: WiFi State Monitoring & Auto-Reconnect Task (Priority 2 - Medium)
    xTaskCreatePinnedToCore(
        vWifiTask,
        "WifiTask",
        4096,
        NULL,
        2,
        NULL,
        1
    );

    // Task 4: Web Server & System Monitor Task (Priority 1 - Low)
    xTaskCreatePinnedToCore(
        vWebMonitorTask,
        "WebMonitorTask",
        4096,
        NULL,
        1,
        NULL,
        1
    );

    // Task 5: Log & Command Processing Task (Priority 2 - Medium)
    xTaskCreatePinnedToCore(
        vLogTask,
        "LogTask",
        4096,
        NULL,
        2,
        NULL,
        1
    );
}
