#ifndef CORE_TIEN_INTERPRETER_H
#define CORE_TIEN_INTERPRETER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <vector>
#include "TienInterpreter.h"

#define TIEN_NAME_LEN   32   /* Size of a script name, terminator included */

typedef struct {
    char name[TIEN_NAME_LEN]; /* Name of the script (the file path for tien_run_file); also its path for 'import' */
    ti_handle_t handle; /* runtime handle used to stop task safely (stale handles are rejected) */
    char *source_code;  /* own copy of the source, freed right after compiling */
} tien_task_t;

/** @brief Executes a Tien script from a LittleFS file path. Returns true when the script task was started. */
bool tien_run_file(const char *path);

/**
 * @brief Executes a Tien script directly from a source code string buffer with a given task name.
 * @return true when the script task was started (a compile error is reported later by the task itself).
 */
bool tien_run_script(const char *name, const char *source_code);

/** @brief Stops a running Tien interpreter task by name. Returns false when no script has that name. */
bool tien_stop(const char *name);

/**
 * @brief Copies the names of the running scripts.
 * @param names Receives up to max names.
 * @param max Capacity of names.
 * @return Number of names copied.
 */
int tien_get_names(char names[][TIEN_NAME_LEN], int max);

/** @brief Initializes the Tien interpreter module and registers CLI commands. */
void tien_init(void);

#endif // CORE_TIEN_INTERPRETER_H

