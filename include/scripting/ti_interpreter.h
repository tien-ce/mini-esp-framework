#ifndef CORE_TIEN_INTERPRETER_H
#define CORE_TIEN_INTERPRETER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <vector>
#include "TienInterpreter.h"
#include "include/ti_runtime.h"
typedef struct {
    char name[32]; /* Name of script used to manage */
    ti_runtime_t *runtime; /* runtime pointer used to stop task safely*/
    char *source_code; // pointer use to allocate new code, avoid code is deleted in build time
} ti_task_t;

/** @brief Executes a Tien script from a LittleFS file path. */
void tien_run_file(const char *path);

/** @brief Executes a Tien script directly from a source code string buffer with a given task name. */
void tien_run_script(const char *name, const char *source_code);

/** @brief Stops and deletes a running Tien interpreter task by name. */
void tien_stop(const char *name);

/** @brief Initializes the Tien interpreter module and registers CLI commands. */
void tien_init(void);

#endif // CORE_TIEN_INTERPRETER_H

