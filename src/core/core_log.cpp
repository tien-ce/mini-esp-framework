/*---- INCLUDES ----*/
#include "core/core_log.h"

/*---- STATIC VARIABLES ----*/
/** @brief Holds the current log level threshold to filter outgoing messages. */
static LogLevel currentLogLevel = LOG_LEVEL_DEBUG;

/** @brief FreeRTOS mutex to ensure thread-safe writes to Serial and external sinks. */
static SemaphoreHandle_t logMutex = NULL;

/** @brief Registered callbacks to forward log messages to external services (e.g., WebSockets). */
#define MAX_LOG_CALLBACKS 5
static LogOutputCallback logCallbacks[MAX_LOG_CALLBACKS] = {NULL};

/*---- PUBLIC FUNCTIONS ----*/

/**
 * @brief Initializes the core logging system.
 */
void core_log_init() {
    // Initialize the mutex only if it hasn't been created yet
    if (logMutex == NULL) {
        logMutex = xSemaphoreCreateMutex();
    }
    // Start hardware serial for standard log output
    Serial.begin(115200);
}

/**
 * @brief Sets the current log filtering level.
 */
void core_log_set_level(LogLevel level) {
    currentLogLevel = level;
}

/**
 * @brief Retrieves the current log filtering level.
 */
LogLevel core_log_get_level() {
    return currentLogLevel;
}

/**
 * @brief Registers a callback function for log output.
 */
void core_log_register_cb(LogOutputCallback cb) {
    // Find an empty slot in the callback array and register the new callback
    for (int i = 0; i < MAX_LOG_CALLBACKS; i++) {
        if (logCallbacks[i] == NULL) {
            logCallbacks[i] = cb;
            return;
        }
    }
    LOG_ERROR_STR("Full callback slot for log output");
}

/**
 * @brief Core function to print log messages.
 */
void core_log_print(const String &msg, LogLevel level) {
    // Drop messages that are below the current log level threshold
    if (level < currentLogLevel) return;

    // Attempt to acquire the mutex for thread-safe Serial printing
    if (logMutex != NULL && xSemaphoreTake(logMutex, portMAX_DELAY) == pdTRUE) {
        // Output with appropriate ANSI color codes based on severity level
        switch (level) {
            case LOG_LEVEL_DEBUG:
                Serial.println(String(LOG_COLOR_BLUE) + msg + LOG_COLOR_RESET);
                break;
            case LOG_LEVEL_INFO:
                Serial.println(String(LOG_COLOR_GREEN) + msg + LOG_COLOR_RESET);
                break;
            case LOG_LEVEL_WARNING:
                Serial.println(String(LOG_COLOR_YELLOW) + msg + LOG_COLOR_RESET);
                break;
            case LOG_LEVEL_ERROR:
                Serial.println(String(LOG_COLOR_RED) + msg + LOG_COLOR_RESET);
                break;
            default:
                break;
        }
        // Release the mutex after printing
        xSemaphoreGive(logMutex);
    } else {
        // Fallback: If mutex is unavailable (e.g. during early boot before init), 
        // print directly without locking or colors
        Serial.println(msg);
    }

    // Forward the message to any registered external sinks (like WebSockets)
    for (int i = 0; i < MAX_LOG_CALLBACKS; i++) {
        if (logCallbacks[i] != NULL) {
            logCallbacks[i](msg, level);
        }
    }
}
