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
#include <algorithm>
#include <stddef.h>
#include <string.h>
#include <ArduinoJson.h>


/* -------------------------------------------------------------------------- */
/*                              STATIC VARIABLES                              */
/* -------------------------------------------------------------------------- */

static std::vector<ti_task_t> g_tien_tasks;
/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief Removes a task from the active task vector by its script name. */
static void remove_tien_task_by_name(const char *name) {
    if (name == NULL) return;
    auto it = std::find_if(g_tien_tasks.begin(), g_tien_tasks.end(),
        [name](const ti_task_t &task) { return strcmp(task.name, name) == 0; });
    if (it == g_tien_tasks.end()) return;
    g_tien_tasks.erase(it);
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
    /* Remove ti_task_t from tracking list by current task name */
    remove_tien_task_by_name(pcTaskGetName(NULL));
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
    ti_task_t *task = (ti_task_t *)pvParameters;
    if (task != NULL) {
        if (task->source_code != NULL && task->runtime != NULL) {
            /* 1. Compile source code text to AST program */
            ti_program_t *prog = ti_compile(task->source_code);

            /* Free source code buffer immediately to reclaim memory during execution */
            free(task->source_code);
            task->source_code = NULL;

            if (prog != NULL) {
                /* 2. Execute program on dedicated runtime instance */
                ti_execute(task->runtime, prog);

                /* 3. Free AST program allocations after execution completes or halts */
                ti_program_free(prog);
            } else {
                ti_log("[ERROR] Compilation failed for script '%s'\n", task->name);
            }

            /* 4. Teardown runtime instance and free internal allocations */
            ti_runtime_destroy(task->runtime);
            task->runtime = NULL;
        }

        /* 5. Remove task from active script tasks tracking list by name */
        remove_tien_task_by_name(task->name);
        free(task);
    }
    /* 6. Task terminates itself cleanly with no held mutexes or leaked memory */
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

    /* Check if the task already existed by name*/
    for (const auto &task : g_tien_tasks) {
        if (strcmp(task.name, name) == 0) {
            ti_log("[ERROR] Task '%s' already exists\n", name);
            return;
        }
    }

    /* Create new task_t to pass to task, copy name and source code to free independently */
    ti_task_t *task = (ti_task_t *)malloc(sizeof(ti_task_t));
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
    ti_runtime_t *rt = ti_runtime_create();
    if (rt == NULL) {
        free(task->source_code);
        free(task);
        ti_log("[ERROR] Failed to allocate runtime for task '%s'\n", name);
        return;
    }

    /* Assign runtime to task (used for cooperative cancellation via ti_stop) */
    task->runtime = rt;

    /* Create new FreeRTOS task to run in background (pass NULL for handle since we use runtime to stop) */
    BaseType_t ret = xTaskCreate(interpreter_task, name, 8192, (void*)task, 1, NULL);

    /* Check if task creation failed, cleanup all allocated resources to avoid memory leaks */
    if (ret != pdPASS) {
        LOG_ERROR("Failed to create interpreter task: " + String(name));
        ti_log("[ERROR] Failed to create FreeRTOS task '%s'\n", name);
        ti_runtime_destroy(task->runtime);
        free(task->source_code);
        free(task);
        return;
    }

    /* Register task descriptor into global tracking list for web and CLI management */
    g_tien_tasks.push_back(*task);
    LOG_INFO("Tien task '" + String(name) + "' created successfully");
}

void tien_stop(const char *name) {
    if (name == NULL || strlen(name) == 0) return;

    /* Search for running script task by name in active task tracking list */
    auto it = std::find_if(g_tien_tasks.begin(), g_tien_tasks.end(),
        [name](const ti_task_t &task) { return strcmp(task.name, name) == 0; });

    if (it == g_tien_tasks.end()) {
        ti_log("[ERROR] Task '%s' not found\n", name);
        return;
    }

    /*
     * Signal cooperative cancellation via runtime instance:
     * Sets runtime->is_interrupted = true, allowing the interpreter to cleanly
     * abort loops, release all held Mutexes, and safely self-terminate with vTaskDelete(NULL).
     */
    if (it->runtime != NULL) {
        ti_stop(it->runtime);
        ti_log("[INFO] Stop requested for task '%s'\n", name);
        LOG_INFO("Stop requested for Tien task '" + String(name) + "'");
    }
}

void tien_init(void) {
    ti_register_log(tien_log_callback);
    ti_register_fatal(tien_fatal_callback);

    builtins_init();

    register_cmd("tien", tien_run_cmd);
    register_cmd("tien_stop", tien_stop_cmd);
}


