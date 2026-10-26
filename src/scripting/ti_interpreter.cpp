#include "scripting/ti_interpreter.h"
#include "core/core_log.h"
#include "services/logger/log_task.h"
#include "core/file_system.h"
#include "services/web/web_ws.h"
#include "built_in.h"
#include "TienInterpreter.h"
#include "include/ti_runtime.h"
#include "built_in.h"
#include <LittleFS.h>
#include <HTTPClient.h>
#include <freertos/semphr.h>
#include <stddef.h>
#include <string.h>
#include <ArduinoJson.h>


/* -------------------------------------------------------------------------- */
/*                              STATIC VARIABLES                              */
/* -------------------------------------------------------------------------- */

/* Running scripts (the real descriptors, not copies); each one owns a runtime, so at most TI_MAX_RUNTIMES */
static tien_task_t *g_tien_tasks[TI_MAX_RUNTIMES];
/* Guards g_tien_tasks: scripts start from CLI, web and other scripts, and end in their own task */
static SemaphoreHandle_t g_tien_tasks_lock = NULL;

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief Adds a task to the tracking list; returns false if a script with the same name is running. */
static bool add_tien_task(tien_task_t *task) {
    int free_index = -1;
    xSemaphoreTake(g_tien_tasks_lock, portMAX_DELAY);
    for (int i = 0; i < TI_MAX_RUNTIMES; i++) {
        if (g_tien_tasks[i] == NULL) {
            if (free_index < 0) free_index = i;
        } else if (strcmp(g_tien_tasks[i]->name, task->name) == 0) {
            xSemaphoreGive(g_tien_tasks_lock);
            return false;
        }
    }
    /* A free entry always exists here: every entry holds a runtime and ti_create just got one */
    if (free_index >= 0) g_tien_tasks[free_index] = task;
    xSemaphoreGive(g_tien_tasks_lock);
    return free_index >= 0;
}

/** @brief Removes a task from the tracking list (no-op if it is not there). */
static void remove_tien_task(tien_task_t *task) {
    xSemaphoreTake(g_tien_tasks_lock, portMAX_DELAY);
    for (int i = 0; i < TI_MAX_RUNTIMES; i++) {
        if (g_tien_tasks[i] == task) {
            g_tien_tasks[i] = NULL;
            break;
        }
    }
    xSemaphoreGive(g_tien_tasks_lock);
}

/** @brief Returns the script descriptor of the calling FreeRTOS task, or NULL if it is not a script task. */
static tien_task_t *find_current_tien_task(void) {
    TaskHandle_t self = xTaskGetCurrentTaskHandle();
    tien_task_t *found = NULL;
    xSemaphoreTake(g_tien_tasks_lock, portMAX_DELAY);
    for (int i = 0; i < TI_MAX_RUNTIMES; i++) {
        if (g_tien_tasks[i] != NULL && g_tien_tasks[i]->rtos_task == self) {
            found = g_tien_tasks[i];
            break;
        }
    }
    xSemaphoreGive(g_tien_tasks_lock);
    /* Only this task frees its own descriptor, so the pointer stays valid after unlocking */
    return found;
}

/** @brief Drops a task from the list and frees what it owns (program, source copy, runtime, descriptor). */
static void release_tien_task(tien_task_t *task) {
    remove_tien_task(task);
    if (task->prog != NULL) ti_program_free(task->prog);
    if (task->handle != TI_INVALID_HANDLE) ti_destroy(task->handle);
    free(task->source_code);
    free(task);
}

/** @brief Validates if given file path has .ti script file extension. */
static bool has_tien_extension(const char *path) {
    if (path == NULL) return false;
    
    size_t len = strlen(path);
    const char *ext = ".ti";
    size_t ext_len = strlen(ext);
    
    if (len < ext_len) return false;
    
    return strcmp(path + len - ext_len, ext) == 0;
}

/** @brief Logging callback function registered to TienInterpreter to route output to Serial and Web. */
static void tien_log_callback(const char *fmt, va_list args) {
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    Serial.print(buffer);
    tien_out_to_ws(buffer);
}

/** @brief Fatal error callback registered to TienInterpreter to log and terminate task. */
static void tien_fatal_callback(void) {
    LOG_ERROR("Interpreter fatal error occurred!");
    tien_out_to_ws("[Fatal Error]: Interpreter execution halted.\n");

    /* This task never returns to interpreter_task, so its cleanup must happen here */
    tien_task_t *task = find_current_tien_task();
    if (task != NULL) {
        release_tien_task(task);
    }
    vTaskDelete(NULL);
}

/** @brief CLI command handler for 'tien <filepath.ti>' command. */
static void tien_run_cmd(const String &args) {
    String cleanArg = args;
    cleanArg.trim();
    if (cleanArg.length() == 0) return;

    String firstArg;
    int spaceIndex = cleanArg.indexOf(' ');
    if (spaceIndex != -1) {
        firstArg = cleanArg.substring(0, spaceIndex);
    } else {
        firstArg = cleanArg;
    }
    tien_run_file(firstArg.c_str());
}

/** @brief CLI command handler for 'tien_stop <name>' command. */
static void tien_stop_cmd(const String &args) {
    String cleanArg = args;
    cleanArg.trim();
    if (cleanArg.length() == 0) return;
    tien_stop(cleanArg.c_str());
}

/** @brief FreeRTOS task function to execute Tien script in background task. */
static void interpreter_task(void *pvParameters) {
    tien_task_t *task = (tien_task_t *)pvParameters;
    if (task != NULL) {
        if (task->source_code != NULL && task->handle != TI_INVALID_HANDLE) {
            /* 1. Compile source code text to AST program (a syntax error ends in tien_fatal_callback) */
            task->prog = ti_compile(task->source_code);

            /* Free source code buffer immediately to reclaim memory during execution */
            free(task->source_code);
            task->source_code = NULL;

            if (task->prog != NULL) {
                /* 2. Execute program on dedicated runtime instance (a runtime error ends in tien_fatal_callback) */
                ti_execute(task->handle, task->prog);
            } else {
                ti_log("[ERROR] Compilation failed for script '%s'\n", task->name);
            }
        }

        /* 3. Free program, runtime and descriptor, and remove the task from the tracking list */
        release_tien_task(task);
    }
    /* 4. Task terminates itself cleanly with no held mutexes or leaked memory */
    vTaskDelete(NULL);
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

void tien_run_file(const char *path) {
    if (!has_tien_extension(path)) {
        LOG_ERROR(String(path) + " is not a .ti file");
        return;
    }
    char *buffer = NULL;
    size_t read_bytes = 0;
    FsResult_t ret = read_file(path, &buffer, &read_bytes);
    if (ret != FS_OK) {
        LOG_ERROR("Read error: " + String(file_system_strerror(ret)));
        return;
    }
    /* Use path like the name of script task */
    tien_run_script(path, buffer);
    free(buffer);
}

void tien_run_script(const char *name, const char *source_code) {
    /* Check the NULL pointer for safely */
    if (name == NULL || strlen(name) == 0 || source_code == NULL) {
        ti_log("[ERROR] Invalid script name or source code\n");
        return;
    }

    /* Create new task_t to pass to task, copy name and source code to free independently (calloc: prog and rtos_task start NULL) */
    tien_task_t *task = (tien_task_t *)calloc(1, sizeof(tien_task_t));
    if (task == NULL) {
        ti_log("[ERROR] Memory allocation failed for task '%s'\n", name);
        return;
    }

    /* Copy source code and name to avoid depent or the resource passed */
    strncpy(task->name, name, sizeof(task->name) - 1);
    task->name[sizeof(task->name) - 1] = '\0';
    task->source_code = strdup(source_code);
    if (task->source_code == NULL) {
        free(task);
        ti_log("[ERROR] Memory allocation failed for task '%s'\n", name);
        return;
    }

    /* Create new runtime instance */
    ti_handle_t rt = ti_create();
    if (rt == TI_INVALID_HANDLE) {
        free(task->source_code);
        free(task);
        ti_log("[ERROR] Failed to allocate runtime for task '%s'\n", name);
        return;
    }

    /* Assign runtime to task (used for cooperative cancellation via ti_stop) */
    task->handle = rt;

    /* Register before the task starts: its fatal callback looks itself up in the list (name check is atomic with it) */
    if (!add_tien_task(task)) {
        ti_log("[ERROR] Task '%s' already exists\n", name);
        ti_destroy(task->handle);
        free(task->source_code);
        free(task);
        return;
    }

    /* Create new FreeRTOS task to run in background (rtos_task is set before the task can run) */
    BaseType_t ret = xTaskCreate(interpreter_task, name, 8192, (void*)task, 1, &task->rtos_task);

    /* Check if task creation failed, cleanup all allocated resources to avoid memory leaks */
    if (ret != pdPASS) {
        LOG_ERROR("Failed to create interpreter task: " + String(name));
        ti_log("[ERROR] Failed to create FreeRTOS task '%s'\n", name);
        remove_tien_task(task);
        ti_destroy(task->handle);
        free(task->source_code);
        free(task);
        return;
    }

    /* The task may already be finished and freed here, so only use 'name' from now on */
    LOG_INFO("Tien task '" + String(name) + "' created successfully");
}

void tien_stop(const char *name) {
    if (name == NULL || strlen(name) == 0) return;

    /* Search for running script task by name; copy the handle because the task may free itself after unlock */
    bool found = false;
    ti_handle_t handle = TI_INVALID_HANDLE;
    xSemaphoreTake(g_tien_tasks_lock, portMAX_DELAY);
    for (int i = 0; i < TI_MAX_RUNTIMES; i++) {
        if (g_tien_tasks[i] != NULL && strcmp(g_tien_tasks[i]->name, name) == 0) {
            found = true;
            handle = g_tien_tasks[i]->handle;
            break;
        }
    }
    xSemaphoreGive(g_tien_tasks_lock);

    if (!found) {
        ti_log("[ERROR] Task '%s' not found\n", name);
        return;
    }

    /*
     * Signal cooperative cancellation via runtime instance:
     * Sets runtime->is_interrupted = true, allowing the interpreter to cleanly
     * abort loops, release all held Mutexes, and safely self-terminate with vTaskDelete(NULL).
     */
    if (handle != TI_INVALID_HANDLE) {
        ti_stop(handle);
        ti_log("[INFO] Stop requested for task '%s'\n", name);
        LOG_INFO("Stop requested for Tien task '" + String(name) + "'");
    }
}

void tien_init(void) {
    g_tien_tasks_lock = xSemaphoreCreateMutex();
    ti_register_log(tien_log_callback);
    ti_register_fatal(tien_fatal_callback);

    builtins_init();

    register_cmd("tien", tien_run_cmd);
    register_cmd("tien_stop", tien_stop_cmd);
}


