/**
 * @file sensor_count_QA.cpp
 * @brief Main Entry Point for ESP32-S3 Industrial Sensor Counter.
 * 
 * Modular System Overview:
 * - config_manager: NVS Flash preferences, global state & mutex management.
 * - logger: Thread-safe WebSocket & Serial logging system with helper macros.
 * - wifi_task: Dedicated WiFi initialization, event handling, and reconnect FreeRTOS task.
 * - index_html: Embedded Web UI (HTML, CSS, JavaScript).
 * - web_server: AsyncWebServer REST endpoints & WebSocket communications.
 * - freertos_tasks: Prioritized FreeRTOS tasks (Sensor, WiFi, WebMonitor, Command).
 */

#include <Arduino.h>
#include "Header_sensor.h"
#include "config_manager.h"
#include "log_task.h"
#include "wifi_task.h"
#include "web_server.h"
#include "freertos_tasks.h"

// ==================== ARDUINO INITIALIZATION ====================
void setup() {
    Serial.begin(9600);
    delay(2000);

    Serial.println("\n=== Industrial SENSOR Counter (FreeRTOS Architecture) ===");
    Serial.println("Firmware Version: " + String(FIRMWARE_VERSION));

    // 1. Initialize Log Task & Log Mutex
    initLogTask();

    // 2. Load Configuration from NVS Flash Storage
    loadConfig();

    // 3. Initialize Hardware GPIO Pin
    pinMode(IN_1, INPUT);

    // 4. Connect to WiFi Network
    setup_wifi();
    
    // 5. Initialize Async Web Server & REST Endpoints
    if (WiFi.status() == WL_CONNECTED) {
        setupWebServer();
    }

    resetSensorCount();

    // 6. Spawn FreeRTOS Multitasking Architecture
    initFreeRTOSTasks();

    Serial.println("=== Initialization Complete & Multitasking System Running ===\n");
}

// ==================== ARDUINO MAIN LOOP ====================
void loop() {
    // Main loop remains idle; work is handled asynchronously by FreeRTOS tasks
    vTaskDelay(pdMS_TO_TICKS(1000));
}
