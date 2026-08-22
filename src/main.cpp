/**
 * @file main.cpp
 * @brief Main Entry Point for Core Engine Framework.
 * 
 * Modular System Overview:
 * - core_engine: CoreEngine_Start() bootstrapping entry point.
 * - config_manager: NVS Flash preferences, global state & mutex management.
 * - logger: Thread-safe WebSocket & Serial logging system with helper macros.
 * - wifi_task: Dedicated WiFi initialization, event handling, and reconnect FreeRTOS task.
 * - web_server: AsyncWebServer REST endpoints & WebSocket communications.
 * - drivers: Application sensor drivers & tasks.
 */

#include <Arduino.h>
#include "core/core_engine.h"

/* -------------------------------------------------------------------------- */
/*                            FORWARD DECLARATIONS                            */
/* -------------------------------------------------------------------------- */

extern void initSensorTasks(); // Forward declaration for sensor task initialization

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Arduino setup entry point. Initializes Core Engine infrastructure and system tasks.
 * @param None
 * @return None
 */
void setup() {
    // 1. Boot Core Engine Infrastructure (LittleFS, Log, WiFi, WebServer, Core Tasks)
    CoreEngine_Start();
    // 2. Application Layer: Initialize Hardware GPIO & Sensor Tasks
    //initSensorTasks();

    Serial.println("=== Initialization Complete & Multitasking System Running ===\n");
}

/**
 * @brief Arduino main loop. Remains idle as tasks are scheduled by FreeRTOS.
 * @param None
 * @return None
 */
void loop() {
    // Main loop remains idle; work is handled asynchronously by FreeRTOS tasks
    vTaskDelay(pdMS_TO_TICKS(1000));
}
