#ifndef CORE_LOG_H
#define CORE_LOG_H

/*---- INCLUDES ----*/
#include <Arduino.h>

/*---- MACROS & DEFINES ----*/
/* ANSI Color Codes for Terminal/Serial */
#define LOG_COLOR_RESET   "\033[0m"
#define LOG_COLOR_BLACK   "\033[30m"
#define LOG_COLOR_RED     "\033[31m"
#define LOG_COLOR_GREEN   "\033[32m"
#define LOG_COLOR_YELLOW  "\033[33m"
#define LOG_COLOR_BLUE    "\033[34m"

/*---- ENUMS & TYPES ----*/
/**
 * @brief Log severity levels for filtering system output.
 */
enum LogLevel {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR
};

/**
 * @brief Callback function type for external log sinks (e.g., WebSockets).
 * @param msg The log message string.
 * @param level The log severity level.
 */
typedef void (*LogOutputCallback)(const String &msg, LogLevel level);

/*---- PUBLIC FUNCTIONS ----*/
/**
 * @brief Initializes the core logging system.
 * 
 * Sets up the required mutexes and begins the Serial interface at the default baud rate.
 */
void core_log_init();

/**
 * @brief Sets the current log filtering level.
 * @param level The minimum log level required for a message to be output.
 */
void core_log_set_level(LogLevel level);

/**
 * @brief Retrieves the current log filtering level.
 * @return LogLevel The current log level.
 */
LogLevel core_log_get_level();

/**
 * @brief Core function to print log messages.
 * 
 * Thread-safe logging function that outputs colored messages to Serial
 * and forwards them to any registered callback.
 * 
 * @param msg The log message string.
 * @param level The log severity level.
 */
void core_log_print(const String &msg, LogLevel level = LOG_LEVEL_INFO);

/**
 * @brief Registers a callback function for log output.
 * 
 * Allows external services (like the WebSocket logger) to receive log messages.
 * 
 * @param cb The callback function to register.
 */
void core_log_register_cb(LogOutputCallback cb);

/*---- LOGGING MACROS ----*/
#define LOG(msg)            core_log_print(String(msg), LOG_LEVEL_INFO)
#define LOG_DEBUG(msg)      core_log_print(String(msg), LOG_LEVEL_DEBUG)
#define LOG_INFO(msg)       core_log_print(String(msg), LOG_LEVEL_INFO)
#define LOG_WARNING(msg)    core_log_print(String(msg), LOG_LEVEL_WARNING)
#define LOG_ERROR(msg)      core_log_print(String(msg), LOG_LEVEL_ERROR)

#define LOG_DEBUG_STR(fmt, ...) do { \
    char _log_buf[256]; \
    snprintf(_log_buf, sizeof(_log_buf), fmt, ##__VA_ARGS__); \
    LOG_DEBUG(String(_log_buf)); \
} while(0)

#define LOG_INFO_STR(fmt, ...) do { \
    char _log_buf[256]; \
    snprintf(_log_buf, sizeof(_log_buf), fmt, ##__VA_ARGS__); \
    LOG_INFO(String(_log_buf)); \
} while(0)

#define LOG_WARNING_STR(fmt, ...) do { \
    char _log_buf[256]; \
    snprintf(_log_buf, sizeof(_log_buf), fmt, ##__VA_ARGS__); \
    LOG_WARNING(String(_log_buf)); \
} while(0)

#define LOG_ERROR_STR(fmt, ...) do { \
    char _log_buf[256]; \
    snprintf(_log_buf, sizeof(_log_buf), fmt, ##__VA_ARGS__); \
    LOG_ERROR(String(_log_buf)); \
} while(0)

#endif // CORE_LOG_H
