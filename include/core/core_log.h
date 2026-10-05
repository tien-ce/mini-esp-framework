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
 * @note Invoked from the internal log dispatch task's context (see
 *       core_log_init()), NOT from the task that originally called
 *       core_log_print()/LOG(). Implementations must be non-blocking and
 *       must not assume anything about the producer's task/stack.
 */
typedef void (*LogOutputCallback)(const String &msg, LogLevel level);

/*---- PUBLIC FUNCTIONS ----*/
/**
 * @brief Initializes the core logging system.
 *
 * Begins the Serial interface, creates the internal log message queue, and
 * starts a dedicated low-priority FreeRTOS task that owns all blocking log
 * I/O (colored Serial output and registered-callback dispatch). After this
 * call, core_log_print() / the LOG* macros only copy the message into the
 * queue - they never touch Serial or run a callback themselves, so logging
 * from any task stays fast and non-blocking.
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
 * @brief Submits a plain C-string message for asynchronous logging.
 *
 * This is the primary, allocation-free entry point. It only checks the
 * message's level against the current threshold and copies the text into
 * the internal log queue - it never performs Serial I/O or invokes
 * callbacks itself (that happens later, on the dedicated dispatch task
 * started by core_log_init()), so it is always safe/fast to call from any
 * task, including ones that must never block on I/O.
 *
 * @param msg   NUL-terminated message text. Longer than the internal
 *              buffer gets truncated (see LOG_MSG_MAX_LEN in core_log.cpp).
 * @param level Severity level this message is logged at.
 */
void core_log_print(const char *msg, LogLevel level = LOG_LEVEL_INFO);

/**
 * @brief String overload of core_log_print(), kept for backward compatibility.
 *
 * Existing call sites that already hold a C++ `String` can keep passing it
 * directly; this just forwards to the `const char*` overload above via
 * `String::c_str()`, so there is exactly one real implementation.
 *
 * @param msg   Message text as a C++ String.
 * @param level Severity level this message is logged at.
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
/*
 * These pass the message straight through to core_log_print() with NO
 * String(...) wrapper. Overload resolution then picks whichever
 * core_log_print() matches what the caller handed in:
 *   - a string literal, char*, or char[] buffer -> const char* overload
 *   - an existing String variable                -> const String& overload
 * This keeps the common case (string literals / snprintf buffers, see the
 * _STR macros below) allocation-free, while still letting old call sites
 * that build a String keep working unchanged.
 */
#define LOG(msg)            core_log_print(msg, LOG_LEVEL_INFO)
#define LOG_DEBUG(msg)      core_log_print(msg, LOG_LEVEL_DEBUG)
#define LOG_INFO(msg)       core_log_print(msg, LOG_LEVEL_INFO)
#define LOG_WARNING(msg)    core_log_print(msg, LOG_LEVEL_WARNING)
#define LOG_ERROR(msg)      core_log_print(msg, LOG_LEVEL_ERROR)

#define LOG_DEBUG_STR(fmt, ...) do { \
    char _log_buf[256]; \
    snprintf(_log_buf, sizeof(_log_buf), fmt, ##__VA_ARGS__); \
    LOG_DEBUG(_log_buf); \
} while(0)

#define LOG_INFO_STR(fmt, ...) do { \
    char _log_buf[256]; \
    snprintf(_log_buf, sizeof(_log_buf), fmt, ##__VA_ARGS__); \
    LOG_INFO(_log_buf); \
} while(0)

#define LOG_WARNING_STR(fmt, ...) do { \
    char _log_buf[256]; \
    snprintf(_log_buf, sizeof(_log_buf), fmt, ##__VA_ARGS__); \
    LOG_WARNING(_log_buf); \
} while(0)

#define LOG_ERROR_STR(fmt, ...) do { \
    char _log_buf[256]; \
    snprintf(_log_buf, sizeof(_log_buf), fmt, ##__VA_ARGS__); \
    LOG_ERROR(_log_buf); \
} while(0)

#endif // CORE_LOG_H
