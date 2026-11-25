#ifndef LOG_TASK_H
#define LOG_TASK_H

#include <Arduino.h>
#include "cmd.h"
#include "core/core_log.h"

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

/** @brief Starts the FreeRTOS Log & Command processing task. */
void log_task_start(void);

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

/** @brief Posts incoming command to vLogTask queue. */
void postIncomingCommand(const String &cmdText);

/* -------------------------------------------------------------------------- */
/*                               STRING CONSTANTS                             */
/* -------------------------------------------------------------------------- */
#include "cmd.h"
#endif // LOG_TASK_H
