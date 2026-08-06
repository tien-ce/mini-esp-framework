#ifndef LOG_TASK_H
#define LOG_TASK_H

#include <Arduino.h>

/* -------------------------------------------------------------------------- */
/*                                  CONSTANTS                                 */
/* -------------------------------------------------------------------------- */

/* ANSI Color Codes for Terminal/Serial */
#define LOG_COLOR_RESET   "\033[0m"
#define LOG_COLOR_BLACK   "\033[30m"
#define LOG_COLOR_RED     "\033[31m"
#define LOG_COLOR_GREEN   "\033[32m"
#define LOG_COLOR_YELLOW  "\033[33m"
#define LOG_COLOR_BLUE    "\033[34m"

/* HTML Color Wrappers for Web (WebSocket) */
#define WEB_COLOR_RESET   "</span>"
#define WEB_COLOR_RED     "<span style='color:#ff4d4d;'>"
#define WEB_COLOR_GREEN   "<span style='color:#00ff00;'>"
#define WEB_COLOR_YELLOW  "<span style='color:#ffff00;'>"
#define WEB_COLOR_CYAN    "<span style='color:#00ffff;'>"
#define WEB_COLOR_WHITE   "<span style='color:#ffffff;'>"
#define WEB_COLOR_GRAY    "<span style='color:#aaaaaa;'>"

/* Logging Helper Macros */
#define LOG(msg)            logPrint(String(msg), LOG_LEVEL_INFO);
#define LOG_DEBUG(msg)      logPrint(String(msg), LOG_LEVEL_DEBUG);
#define LOG_INFO(msg)       logPrint(String(msg), LOG_LEVEL_INFO);
#define LOG_WARNING(msg)    logPrint(String(msg), LOG_LEVEL_WARNING);
#define LOG_ERROR(msg)      logPrint(String(msg), LOG_LEVEL_ERROR);

/* -------------------------------------------------------------------------- */
/*                             ENUMS & DATA TYPES                             */
/* -------------------------------------------------------------------------- */

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
 * @brief Callback function type for command handlers.
 * @param args Arguments passed along with the command.
 */
typedef void (*CommandHandlerFunc)(const String &args);

/**
 * @brief Command packet structure passed via FreeRTOS queue.
 */
struct CommandPacket {
    char text[128];
};

/* -------------------------------------------------------------------------- */
/*                            EXTERNAL API FUNCTIONS                          */
/* -------------------------------------------------------------------------- */

/** @brief FreeRTOS task for log and command processing. */
void vLogTask(void *pvParameters);

/** @brief Thread-safe logging function for Serial and Web sockets. */
void logPrint(const String &msg, LogLevel level = LOG_LEVEL_INFO);

/** @brief Registers command string and handler callback. */
bool register_cmd(const String &name, CommandHandlerFunc handler);

/** @brief Enables Web WebSocket log output. */
void setWebLogReady();

/** @brief Posts incoming command to vLogTask queue. */
void postIncomingCommand(const String &cmdText);

/* -------------------------------------------------------------------------- */
/*                               STRING CONSTANTS                             */
/* -------------------------------------------------------------------------- */
#define CMD_SET_LOG_LEVEL   "CMD_SET_LEVEL"
#define CMD_GET_LOG_LEVEL   "CMD_GET_LEVEL"
#define CMD_LIST_LOG_LEVEL  "CMD_LIST_LEVEL"
#define CMD_RESTART         "ESP32_RESTART"
#endif // LOG_TASK_H


