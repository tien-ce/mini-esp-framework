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
#include "services/services.h"
#include "scripting/ti_interpreter.h"

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Arduino setup entry point. Initializes Core Engine infrastructure and system tasks.
 * @param None
 * @return None
 */
void setup() {
    // 1. Boot Core Engine Infrastructure (Filesystem, NVS, System State Group)
    CoreEngine_Start();

    // 2. Boot Services Layer (Log, WiFi, WebServer, MQTT)
    services_start();

    // 3. Initialize Scripting Runtime (Tien Interpreter & builtins)
    tien_init();

    // 4. Boot complete: every service and the script hook exist, release the
    //    tasks waiting on SYS_NORMAL (dispatcher, WiFi)
    CoreState_SetMode(SYS_NORMAL);

    // 5. Application Layer: run user init script
    tien_run_file("/init.ti");

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
