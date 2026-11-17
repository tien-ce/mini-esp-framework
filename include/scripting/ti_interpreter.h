#ifndef CORE_TIEN_INTERPRETER_H
#define CORE_TIEN_INTERPRETER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <vector>
#include "TienInterpreter.h"
typedef struct {
    char name[32];      /* Name of the script (the file path for tien_run_file); also its path for 'import' */
    ti_handle_t handle; /* runtime handle used to stop task safely (stale handles are rejected) */
    char *source_code;  /* own copy of the source, freed right after compiling */
} tien_task_t;

/** @brief Executes a Tien script from a LittleFS file path. */
void tien_run_file(const char *path);

/** @brief Executes a Tien script directly from a source code string buffer with a given task name. */
void tien_run_script(const char *name, const char *source_code);

/** @brief Stops and deletes a running Tien interpreter task by name. */
void tien_stop(const char *name);

/** @brief Initializes the Tien interpreter module and registers CLI commands. */
void tien_init(void);

#endif // CORE_TIEN_INTERPRETER_H

