#ifndef LOG_TASK_H
#define LOG_TASK_H

#include <Arduino.h>

/* -------------------------------------------------------------------------- */
/*                                  CONSTANTS                                 */
/* -------------------------------------------------------------------------- */
#define SERIAL_BAUDRATE    9600 

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
 * @brief Identifies the source of incoming commands (Serial or Web).
 */
enum CommandSource {
    CMD_SOURCE_SERIAL,
    CMD_SOURCE_WEB
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
    CommandSource source;
};

/* -------------------------------------------------------------------------- */
/*                            EXTERNAL API FUNCTIONS                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initializes the logging system, Serial interface, and default commands.
 */
void initLogTask();

/**
 * @brief FreeRTOS Task function for log and command processing.
 */
void vLogTask(void *pvParameters);

/**
 * @brief Thread-safe logging function that outputs messages to Serial and Web sockets.
 * @param msg The message string to log.
 * @param level Log severity level (default: LOG_LEVEL_INFO).
 */
void logPrint(const String &msg, LogLevel level = LOG_LEVEL_INFO);

/**
 * @brief Registers a command string and its associated callback handler into the command map.
 * @param name The command identifier name string.
 * @param handler Function pointer callback to execute when the command is received.
 * @return true if successfully registered, false if command already exists.
 */
bool register_cmd(const String &name, CommandHandlerFunc handler);

/**
 * @brief Enables Serial log output.
 */
void setSerialLogReady();

/**
 * @brief Enables Web WebSocket log output.
 */
void setWebLogReady();

/**
 * @brief Thread-safe function to post incoming command from Serial or Web to vLogTask queue.
 * @param cmdText Command text string.
 * @param source Origin source (CMD_SOURCE_SERIAL or CMD_SOURCE_WEB).
 */
void postIncomingCommand(const String &cmdText, CommandSource source);

#endif // LOG_TASK_H
