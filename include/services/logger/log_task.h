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

/** @brief Registers command string and handler callback. */
bool register_cmd(const String &name, CommandHandlerFunc handler);

/** @brief Enables Web WebSocket log output. */
void setWebLogReady();

/** @brief Posts incoming command to vLogTask queue. */
void postIncomingCommand(const String &cmdText);

/* -------------------------------------------------------------------------- */
/*                               STRING CONSTANTS                             */
/* -------------------------------------------------------------------------- */
#include "cmd.h"
#endif // LOG_TASK_H
