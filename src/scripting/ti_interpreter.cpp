#include "scripting/ti_interpreter.h"
#include "config.h"
#include "core/core_log.h"
#include "services/logger/log_task.h"
#include "core/file_system.h"
#include "scripting/builtins/built_in.h"
#include "scripting/module/module_manager.h"
#include "TienInterpreter.h"
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

/** @brief Validates if given file path has .ti script file extension. */
static bool has_tien_extension(const char *path) {
    if (path == NULL) return false;
    
    size_t len = strlen(path);
    const char *ext = ".ti";
    size_t ext_len = strlen(ext);
    
    if (len < ext_len) return false;
    
    return strcmp(path + len - ext_len, ext) == 0;
}

/** @brief Logging callback function registered to TienInterpreter. */
static void tien_log_callback(const char *fmt, va_list args) {
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    LOG_ERROR(buffer);
}

/**
 * @brief Normalize an absolute path in place: drop empty and "." segments, apply ".." to the previous segment.
 *        "/project/lib/sensor/../../config/config.ti" -> "/project/config/config.ti".
 * @param path Absolute path (starts with '/'), rewritten in place (never grows: writing stays behind reading).
 * @return False when ".." goes above "/" or nothing is left.
 */
static bool normalize_path(char *path) {
    size_t r = 0;   /* read index */
    size_t w = 0;   /* write index: path[0..w) is the result, "/seg/seg" */
    while (path[r] != '\0') {
        /* 1. Skip the '/' before the segment, then find its end */
        while (path[r] == '/') r++;
        if (path[r] == '\0') break;
        size_t start = r;
        while (path[r] != '\0' && path[r] != '/') r++;
        size_t len = r - start;

        /* 2. "." stays in place; ".." removes the last written segment */
        if (len == 1 && path[start] == '.') continue;
        if (len == 2 && path[start] == '.' && path[start + 1] == '.') {
            if (w == 0) return false;                /* above the root */
            do { w--; } while (path[w] != '/');      /* back to the '/' of the last segment */
            continue;
        }

        /* 3. Append "/<segment>" */
        path[w++] = '/';
        memmove(path + w, path + start, len);
        w += len;
    }
    path[w] = '\0';
    return w > 0;
}

/**
 * @brief Module loader registered to TienInterpreter: 'import <name>;' of a name no native module has.
 *        The name is a path without ".ti", relative to the directory of the importing file
 *        ("/" when it has none), or absolute when it starts with '/':
 *        from "/project/lib/sensor/sensor.ti", 'import ../../config/config;' loads "/project/config/config.ti".
 *        The path is normalized, so one file always has one path (the library loads a path only once).
 * @param importer_path Path of the importing file (the main script's task name, or a module path returned here).
 * @param name Module name written after 'import'.
 * @param text_out Receives the module source, only on TI_OK (the library frees it with the registered free).
 * @param path_out Receives the module path, only on TI_OK (the library frees it with the registered free).
 * @note Plain malloc / free: the library's free is free (with or without PSRAM), which frees any heap.
 * @return TI_OK, TI_ERR_UNDEFINED if the file does not exist or the path goes above "/" (reported as
 *         'Unknown module'), TI_ERR_NO_MEMORY, or TI_ERR_RUNTIME for another read error.
 */
static ti_status_t tien_module_loader(const char *importer_path, const char *name, char **text_out, char **path_out) {
    /* 1. Base directory: the importing file's directory, '/' included; "/" when it has none or name is absolute */
    const char *dir = "/";
    size_t dir_len = 1;
    if (name[0] == '/') {
        dir_len = 0;
    } else if (importer_path != NULL) {
        const char *slash = strrchr(importer_path, '/');
        if (slash != NULL) {
            dir = importer_path;
            dir_len = (size_t)(slash - importer_path) + 1;
        }
    }

    /* 2. Build "<dir><name>.ti", then resolve "." and ".." */
    size_t name_len = strlen(name);
    char *path = (char *)malloc(1 + dir_len + name_len + sizeof(".ti"));   /* 1: leading '/' if dir lacks it */
    if (path == NULL) return TI_ERR_NO_MEMORY;
    size_t n = 0;
    if (dir_len == 0 || dir[0] != '/') path[n++] = '/';
    memcpy(path + n, dir, dir_len);
    n += dir_len;
    memcpy(path + n, name, name_len);
    n += name_len;
    memcpy(path + n, ".ti", sizeof(".ti"));   /* '\0' included */
    if (!normalize_path(path)) {
        free(path);
        return TI_ERR_UNDEFINED;
    }

    /* 3. Read the file; on failure nothing is handed over, so the path is freed here */
    char *text = NULL;
    unsigned int text_len = 0;
    FsResult_t ret = read_file(path, &text, &text_len);
    if (ret != FS_OK) {
        free(path);
        if (ret == FS_ERR_NOT_FOUND)    return TI_ERR_UNDEFINED;
        if (ret == FS_ERR_ALLOC_FAILED) return TI_ERR_NO_MEMORY;
        return TI_ERR_RUNTIME;
    }

    /* 4. The library owns both buffers from here on */
    *text_out = text;
    *path_out = path;
    return TI_OK;
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
        /* 1. Compile; the name is the program's path, handed to the module loader as main_path on 'import'
         *    (a syntax error is already logged by the library) */
        ti_program_t *prog = ti_compile(task->source_code, task->name);

        /* Free source code buffer immediately to reclaim memory during execution */
        free(task->source_code);
        task->source_code = NULL;

        if (prog != NULL) {
            /* 2. Run; ti_execute destroys the runtime when it returns (also on a runtime error, already logged) */
            ti_execute(task->handle, prog);
            ti_program_free(prog);
        } else {
            LOG_ERROR_STR("Compilation failed for script '%s'", task->name);
            ti_destroy(task->handle);
        }

        /* 3. Remove the task from the tracking list and free the descriptor */
        LOG_INFO("Tien task '" + String(task->name) + "' ended");
        remove_tien_task(task);
        free(task);
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
        LOG_ERROR("Invalid script name or source code");
        return;
    }

    /* Create new task_t to pass to task, copy name and source code to free independently */
    tien_task_t *task = (tien_task_t *)calloc(1, sizeof(tien_task_t));
    if (task == NULL) {
        LOG_ERROR_STR("Memory allocation failed for task '%s'", name);
        return;
    }

    /* Copy source code and name to avoid depent or the resource passed */
    strncpy(task->name, name, sizeof(task->name) - 1);
    task->name[sizeof(task->name) - 1] = '\0';
    task->source_code = strdup(source_code);
    if (task->source_code == NULL) {
        free(task);
        LOG_ERROR_STR("Memory allocation failed for task '%s'", name);
        return;
    }

    /* Create new runtime instance */
    ti_handle_t rt = ti_create();
    if (rt == TI_INVALID_HANDLE) {
        free(task->source_code);
        free(task);
        LOG_ERROR_STR("Failed to allocate runtime for task '%s'", name);
        return;
    }

    /* Assign runtime to task (used for cooperative cancellation via ti_stop) */
    task->handle = rt;

    /* Register before the task starts, so tien_stop can find it as soon as it runs (name check is atomic with it) */
    if (!add_tien_task(task)) {
        LOG_ERROR_STR("Task '%s' already exists", name);
        ti_destroy(task->handle);
        free(task->source_code);
        free(task);
        return;
    }

    /* Create new FreeRTOS task to run in background */
    BaseType_t ret = xTaskCreate(interpreter_task, name, 8192, (void*)task, 1, NULL);

    /* Check if task creation failed, cleanup all allocated resources to avoid memory leaks */
    if (ret != pdPASS) {
        LOG_ERROR_STR("Failed to create FreeRTOS task '%s'", name);
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
        LOG_ERROR_STR("Task '%s' not found", name);
        return;
    }

    /*
     * Signal cooperative cancellation via runtime instance:
     * Sets runtime->is_interrupted = true, allowing the interpreter to cleanly
     * abort loops, release all held Mutexes, and safely self-terminate with vTaskDelete(NULL).
     */
    if (handle != TI_INVALID_HANDLE) {
        ti_stop(handle);
        LOG_INFO("Stop requested for Tien task '" + String(name) + "'");
    }
}

void tien_init(void) {
    g_tien_tasks_lock = xSemaphoreCreateMutex();
    ti_register_log(tien_log_callback);
    ti_register_module_loader(tien_module_loader);   /* before any runtime is created (not locked) */

    #ifdef SYSTEM_USES_PSRAM
    // If use psram, run ti script on ps ram (runtime check: the build flag does not prove the chip exists)
    if (psramFound()) {
        ti_register_allocator(ps_malloc, free, ps_realloc);
    }
    #endif
    builtins_init();
    modules_init();

    register_cmd("tien", tien_run_cmd);
    register_cmd("tien_stop", tien_stop_cmd);
}
