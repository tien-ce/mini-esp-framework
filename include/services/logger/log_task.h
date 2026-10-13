#ifndef LOG_TASK_H
#define LOG_TASK_H

#include <Arduino.h>
#include "core/core_log.h"

/* HTML Color Wrappers for Web (WebSocket) */
#define WEB_COLOR_RESET   "</span>"
#define WEB_COLOR_RED     "<span style='color:#ff4d4d;'>"
#define WEB_COLOR_GREEN   "<span style='color:#00ff00;'>"
#define WEB_COLOR_YELLOW  "<span style='color:#ffff00;'>"
#define WEB_COLOR_CYAN    "<span style='color:#00ffff;'>"
#define WEB_COLOR_WHITE   "<span style='color:#ffffff;'>"
#define WEB_COLOR_GRAY    "<span style='color:#aaaaaa;'>"

/**
 * @brief Callback function type for command handlers.
 * @param args Arguments passed along with the command.
 */
typedef void (*CommandHandlerFunc)(const String &args);

/**
 * @brief Optional descriptive metadata of a command (used by GET /api/commands).
 * @note All members are optional; string members must point to static storage (e.g. literals).
 */
struct CmdMeta {
    const char* info    = "";     /**< Short description, e.g. "Set the log level". */
    const char* usage   = "";     /**< Usage line, e.g. "CMD_SET_LEVEL: <level>". */
    const char* args    = "";     /**< Comma-separated suggested arguments, e.g. "DEBUG,INFO". */
    const char* aliases = "";     /**< Comma-separated alias names, e.g. "WIFI_SSID,SSID_NAME". */
    bool        files   = false;  /**< true when the argument is a script path on LittleFS. */
};

/**
 * @brief Command packet structure passed via FreeRTOS queue.
 */
struct CommandPacket {
    char text[512];
};

/* -------------------------------------------------------------------------- */
/*                            EXTERNAL API FUNCTIONS                          */
/* -------------------------------------------------------------------------- */

/** @brief Early initialization of Serial hardware and logging mutex. */
void log_task_init(void);

/** @brief FreeRTOS task for log and command processing. */
void vLogTask(void *pvParameters);

/**
 * @brief Registers a command and, automatically, every alias listed in meta.aliases.
 * @param name Primary command name (case-insensitive, stored upper-case).
 * @param handler Callback executed when the command (or any of its aliases) is received.
 * @param meta Optional description data; defaults keep old call sites valid.
 * @return true if the primary command was registered, false if the name already exists.
 */
bool register_cmd(const String &name, CommandHandlerFunc handler, const CmdMeta &meta = CmdMeta());

/**
 * @brief Writes the list of registered primary commands as JSON ({"commands":[...]}).
 * @param out Destination stream, typically an AsyncResponseStream.
 */
void cmd_list_json(Print &out);

/** @brief Enables Web WebSocket log output. */
void setWebLogReady();

/** @brief Posts incoming command to vLogTask queue. */
void postIncomingCommand(const String &cmdText);

/* -------------------------------------------------------------------------- */
/*                               STRING CONSTANTS                             */
/* -------------------------------------------------------------------------- */
#include "cmd.h"
#endif // LOG_TASK_H
