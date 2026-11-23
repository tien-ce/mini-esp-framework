/**
 * @file system_module.cpp
 * @brief Script module "system": the firmware and the device the script runs on (things that still make sense
 *        on a board with no network; IP, MAC, Wi-Fi belong to a network module).
 *
 *   import system;
 *
 *   Device
 *     dict   system.info()              model, chip, fw_version, sdk_version, flash_size, total_heap, total_psram
 *     int    system.free_heap()         free internal heap in bytes
 *     string system.reset_reason()      why the board last restarted ("POWERON", "PANIC", "SOFTWARE", ...)
 *     void   system.restart()           restart the board (through the log task, like the RESTART command)
 *
 *   Time since boot
 *     int    system.uptime_s()          seconds since boot (32-bit int: about 68 years)
 *     int    system.uptime_ms()         milliseconds since boot as a tick: wraps after about 24.8 days (may be negative),
 *                                       so only use it as the 'since' of elapsed_ms(), never add to it
 *     int    system.elapsed_ms(since)   milliseconds from the tick 'since' to now, correct across the wrap
 *                                       (intervals up to about 24.8 days)
 *
 *   Scripts
 *     bool         system.run(path)     start the .ti file at path as a new script (its name is the path)
 *     bool         system.stop(name)    ask the script with that name to stop; false when none has that name
 *     list string  system.scripts()     names of the running scripts
 *
 *   Firmware signals (central event dispatcher)
 *     void on_signal(int sig) {
 *         if (sig == system.SIG_1SEC) { ... }
 *     }
 *     system.subscribe(on_signal);      subscribe the calling script to EVERY firmware signal
 *     system.unsubscribe();             remove the calling script's subscription
 *     system.SIG_xxx                    signal numbers (Signal_t in core/dispatcher.h)
 *
 * Each script (runtime handle) keeps at most ONE handler; subscribing again replaces it. The handler is called for every
 * signal with ONE argument: the signal number (int), and decides itself what to do with it.
 *   - Signals dispatched with dispatch_signal_sync() (SIG_WEB_POLL, SIG_MQTT_PUBLISH) call the handler with ti_call:
 *     the dispatching task waits until the handler returned.
 *   - Every other signal is posted with ti_post_event(): fire and forget, queued in order.
 *
 * VALUE OWNERSHIP (who frees what):
 *   - argv of the functions: owned by the interpreter, only read. The handler is kept as an OWN copy (val_copy).
 *   - The function value given to ti_call / ti_post_event: borrowed, we keep and free it.
 *   - Argument of ti_call: borrowed, we create and free it. The result is ours too.
 *   - Argument of ti_post_event: ownership ALWAYS moves to ti_post_event (even when it fails), we must not free it.
 * See docs/script_api/modules/system/README.md for which signals are meant to be captured.
 *
 * Adding a signal to Signal_t: add its TI_CONST_INT row below, scripts then use system.SIG_<name>.
 */

/*---- INCLUDES ----*/
#include <Arduino.h>
#include <limits.h>
#include "TienInterpreter.h"
#include "core/dispatcher.h"
#include "core/core_log.h"
#include "core/info.h"
#include "cmd.h"
#include "services/logger/log_task.h"
#include "scripting/ti_interpreter.h"
#include "scripting/module/module_manager.h"

/*---- DEFINES ----*/

#define SYNC_TIMEOUT_MS    200    /* How long a sync dispatch waits for the script to reach a safe point */
#define MAX_SUBS           TI_MAX_RUNTIMES   /* At most one subscription per runtime */

/*---- TYPES ----*/

/** One subscribed script. Free entry: func == NULL. */
typedef struct {
    ti_handle_t handle;    /* Runtime that registered the handler */
    value_t    *func;      /* Handler function value (own copy, freed when replaced or removed) */
} SignalSub_t;

/*---- STATIC FUNCTION PROTOTYPES ----*/

static value_t *system_info(ti_handle_t handle, value_t **argv, int argc);
static value_t *system_free_heap(ti_handle_t handle, value_t **argv, int argc);
static value_t *system_reset_reason(ti_handle_t handle, value_t **argv, int argc);
static value_t *system_restart(ti_handle_t handle, value_t **argv, int argc);
static value_t *system_uptime_s(ti_handle_t handle, value_t **argv, int argc);
static value_t *system_uptime_ms(ti_handle_t handle, value_t **argv, int argc);
static value_t *system_elapsed_ms(ti_handle_t handle, value_t **argv, int argc);
static value_t *system_run(ti_handle_t handle, value_t **argv, int argc);
static value_t *system_stop(ti_handle_t handle, value_t **argv, int argc);
static value_t *system_scripts(ti_handle_t handle, value_t **argv, int argc);
static value_t *system_subscribe(ti_handle_t handle, value_t **argv, int argc);
static value_t *system_unsubscribe(ti_handle_t handle, value_t **argv, int argc);

/*---- STATIC VARIABLES ----*/

static SignalSub_t s_subs[MAX_SUBS];            /* Subscribers, guarded by s_subs_lock */
static SemaphoreHandle_t s_subs_lock = NULL;    /* Written by script threads, read by dispatching tasks */

/*---- PARAMS ----*/

/** elapsed_ms(since). */
static const param_t s_elapsed_params[] = { PARAM_INT("since"), };
/** run(path). */
static const param_t s_run_params[] = { PARAM_STRING("path"), };
/** stop(name). */
static const param_t s_stop_params[] = { PARAM_STRING("name"), };
/** subscribe(handler). */
static const param_t s_subscribe_params[] = { PARAM_FUNC("handler"), };

/*---- FUNCTIONS ----*/

static const ti_builtin_def_t s_system_funcs[] = {
    { "info",         VAL_DICT,   NULL,               0, system_info         },
    { "free_heap",    VAL_INT,    NULL,               0, system_free_heap    },
    { "reset_reason", VAL_STRING, NULL,               0, system_reset_reason },
    { "restart",      VAL_VOID,   NULL,               0, system_restart      },
    { "uptime_s",     VAL_INT,    NULL,               0, system_uptime_s     },
    { "uptime_ms",    VAL_INT,    NULL,               0, system_uptime_ms    },
    { "elapsed_ms",   VAL_INT,    s_elapsed_params,   1, system_elapsed_ms   },
    { "run",          VAL_BOOL,   s_run_params,       1, system_run          },
    { "stop",         VAL_BOOL,   s_stop_params,      1, system_stop         },
    { "scripts",      VAL_LIST,   NULL,               0, system_scripts      },
    { "subscribe",    VAL_VOID,   s_subscribe_params, 1, system_subscribe    },
    { "unsubscribe",  VAL_VOID,   NULL,               0, system_unsubscribe  },
};

/*---- CONSTANTS ----*/

/** Signal numbers, one row per Signal_t value (SIG_MAX is not a signal). */
static const ti_module_const_t s_system_consts[] = {
    TI_CONST_INT("SIG_INIT",              SIG_INIT),
    TI_CONST_INT("SIG_10MS",              SIG_10MS),
    TI_CONST_INT("SIG_100MS",             SIG_100MS),
    TI_CONST_INT("SIG_1SEC",              SIG_1SEC),
    TI_CONST_INT("SIG_WIFI_CONNECTED",    SIG_WIFI_CONNECTED),
    TI_CONST_INT("SIG_WIFI_DISCONNECTED", SIG_WIFI_DISCONNECTED),
    TI_CONST_INT("SIG_WEB_POLL",          SIG_WEB_POLL),
    TI_CONST_INT("SIG_MQTT_PUBLISH",      SIG_MQTT_PUBLISH),
};

/*---- MODULE ----*/

/** The "system" module. */
static const ti_module_t s_system_module = {
    "system",                                                       /* name */
    s_system_funcs,                                                 /* funcs */
    (int)(sizeof(s_system_funcs) / sizeof(s_system_funcs[0])),      /* func_count */
    s_system_consts,                                                /* consts */
    (int)(sizeof(s_system_consts) / sizeof(s_system_consts[0]))     /* const_count */
};

/*---- STATIC FUNCTIONS: HELPERS ----*/

/**
 * @brief Removes the subscription of a runtime (no-op when it has none).
 * @param handle Runtime whose subscription is removed.
 */
static void remove_sub(ti_handle_t handle) {
    value_t *old = NULL;

    xSemaphoreTake(s_subs_lock, portMAX_DELAY);
    for (int i = 0; i < MAX_SUBS; i++) {
        if (s_subs[i].func != NULL && s_subs[i].handle == handle) {
            old = s_subs[i].func;
            s_subs[i].func = NULL;
            break;
        }
    }
    xSemaphoreGive(s_subs_lock);

    val_free(old);   // outside the lock (val_free(NULL) does nothing)
}

/**
 * @brief Stores a dict entry; val_dict_set takes ownership of the value even on failure.
 * @param dict Target dict value.
 * @param key Entry key.
 * @param val Entry value (owned by the dict afterwards; NULL is skipped).
 */
static void info_set(value_t *dict, const char *key, value_t *val) {
    if (val == NULL) return;
    val_dict_set(dict->dict_val, key, val);
}

/*---- STATIC FUNCTIONS: DEVICE ----*/

/**
 * @brief Function system.info(): fixed facts about the board and the firmware (they do not change until the next boot).
 * @param handle Calling runtime handle, used to raise errors.
 * @param argv Unused.
 * @param argc Unused.
 * @return Dict {model, chip, fw_version, sdk_version, flash_size, total_heap, total_psram}; NULL after raising an error.
 */
static value_t *system_info(ti_handle_t handle, value_t **argv, int argc) {
    (void)argv;
    (void)argc;

    value_t *info = val_new_dict();
    if (info == NULL) {
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "system.info: out of memory");
        return NULL;
    }
    info_set(info, "model",       val_new_string(esp_info_get_model()));
    info_set(info, "chip",        val_new_string(esp_info_get_chip_short()));
    info_set(info, "fw_version",  val_new_string(esp_info_get_fw_version()));
    info_set(info, "sdk_version", val_new_string(esp_info_get_sdk_version()));
    info_set(info, "flash_size",  val_new_int((int)esp_info_get_flash_size()));
    info_set(info, "total_heap",  val_new_int((int)esp_info_get_total_heap()));
    info_set(info, "total_psram", val_new_int((int)esp_info_get_total_psram()));
    return info;
}

/**
 * @brief Function system.free_heap(): free internal heap now.
 * @return Int value in bytes.
 */
static value_t *system_free_heap(ti_handle_t handle, value_t **argv, int argc) {
    (void)handle;
    (void)argv;
    (void)argc;
    return val_new_int((int)esp_info_get_free_heap());
}

/**
 * @brief Function system.reset_reason(): why the board last restarted.
 * @return String value, e.g. "POWERON", "PANIC", "SOFTWARE".
 */
static value_t *system_reset_reason(ti_handle_t handle, value_t **argv, int argc) {
    (void)handle;
    (void)argv;
    (void)argc;
    return val_new_string(esp_info_get_reset_reason());
}

/**
 * @brief Function system.restart(): restart the board.
 * @details Posted to the log task as the RESTART command (same path as the web and the console), so the restart is
 *          logged and the log is flushed first. The script keeps running until the board goes down.
 * @return Void value.
 */
static value_t *system_restart(ti_handle_t handle, value_t **argv, int argc) {
    (void)handle;
    (void)argv;
    (void)argc;
    LOG_INFO("Restart requested by a script");
    postIncomingCommand(CMD_RESTART);
    return val_new_void();
}

/*---- STATIC FUNCTIONS: TIME SINCE BOOT ----*/

/**
 * @brief Function system.uptime_s(): seconds since boot.
 * @return Int value.
 */
static value_t *system_uptime_s(ti_handle_t handle, value_t **argv, int argc) {
    (void)handle;
    (void)argv;
    (void)argc;
    return val_new_int((int)esp_info_get_uptime_sec());
}

/**
 * @brief Function system.uptime_ms(): milliseconds since boot, as a tick for elapsed_ms().
 * @details millis() is uint32_t; seen as int it turns negative after about 24.8 days and wraps after about 49.7 days.
 *          Scripts must not do arithmetic on it (TI raises an error on int overflow); elapsed_ms() does the
 *          difference in uint32_t, which is correct across the wrap.
 * @return Int value.
 */
static value_t *system_uptime_ms(ti_handle_t handle, value_t **argv, int argc) {
    (void)handle;
    (void)argv;
    (void)argc;
    return val_new_int((int)(uint32_t)millis());
}

/**
 * @brief Function system.elapsed_ms(int since): milliseconds from the tick 'since' (from uptime_ms()) to now.
 * @details Unsigned subtraction gives the right interval even when millis() wrapped in between. An interval longer
 *          than INT_MAX ms (about 24.8 days) cannot be represented and is returned as INT_MAX.
 * @param argv Arguments: (since). Owned by the interpreter, only read.
 * @return Int value, >= 0.
 */
static value_t *system_elapsed_ms(ti_handle_t handle, value_t **argv, int argc) {
    (void)handle;
    (void)argc;
    uint32_t since = (uint32_t)argv[0]->int_val;
    uint32_t diff = (uint32_t)millis() - since;
    if (diff > (uint32_t)INT_MAX) {
        diff = (uint32_t)INT_MAX;
    }
    return val_new_int((int)diff);
}

/*---- STATIC FUNCTIONS: SCRIPTS ----*/

/**
 * @brief Function system.run(string path): start the .ti file at path as a new script.
 * @details The script's name is its path (also used by its 'import' lines). A compile error is logged by the new
 *          script's task after this returns, so true only means the script was started.
 * @param argv Arguments: (path). Owned by the interpreter, only read.
 * @return Bool value: false when the file is not a .ti file, cannot be read, or a script with that name runs already.
 */
static value_t *system_run(ti_handle_t handle, value_t **argv, int argc) {
    (void)handle;
    (void)argc;
    return val_new_bool(tien_run_file(argv[0]->string_val));
}

/**
 * @brief Function system.stop(string name): ask the script with that name to stop.
 * @details Stopping is cooperative: the script ends at its next safe point (a script may stop itself).
 * @param argv Arguments: (name). Owned by the interpreter, only read.
 * @return Bool value: false when no running script has that name.
 */
static value_t *system_stop(ti_handle_t handle, value_t **argv, int argc) {
    (void)handle;
    (void)argc;
    return val_new_bool(tien_stop(argv[0]->string_val));
}

/**
 * @brief Function system.scripts(): names of the running scripts.
 * @param handle Calling runtime handle, used to raise errors.
 * @return List of strings; NULL after raising an error.
 */
static value_t *system_scripts(ti_handle_t handle, value_t **argv, int argc) {
    (void)argv;
    (void)argc;

    // Step 1: copy the names under the interpreter's lock (scripts may end meanwhile)
    char names[TI_MAX_RUNTIMES][TIEN_NAME_LEN];
    int count = tien_get_names(names, TI_MAX_RUNTIMES);

    // Step 2: build the list; val_list_push takes the value even when it fails
    value_t *list = val_new_list(VAL_STRING);
    if (list == NULL) {
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "system.scripts: out of memory");
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        value_t *name = val_new_string(names[i]);
        if (name == NULL || val_list_push(list->list_val, name) != TI_OK) {
            val_free(list);
            ti_raise_error(handle, TI_ERR_NO_MEMORY, "system.scripts: out of memory");
            return NULL;
        }
    }
    return list;
}

/*---- STATIC FUNCTIONS: SIGNALS ----*/

/**
 * @brief Function system.subscribe(func handler): subscribe the calling script to all signals.
 * @param handle Calling runtime handle (stored so the dispatcher can reach the script later).
 * @param argv Arguments: (handler). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Void value; NULL after raising an error.
 */
static value_t *system_subscribe(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;

    // Step 1: an empty function value ('func f;') has nothing to call
    if (val_func_name(argv[0]) == NULL) {
        ti_raise_error(handle, TI_ERR_INVALID_ARG, "system.subscribe: expects a defined function");
        return NULL;
    }

    // Step 2: own copy of the handler (argv is freed after this call), made before taking the lock
    value_t *func = val_copy(argv[0]);
    if (func == NULL) {
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "system.subscribe: out of memory");
        return NULL;
    }

    // Step 3: replace the runtime's entry, or take a free one
    value_t *old = NULL;
    int slot = -1;
    xSemaphoreTake(s_subs_lock, portMAX_DELAY);
    for (int i = 0; i < MAX_SUBS; i++) {
        if (s_subs[i].func != NULL && s_subs[i].handle == handle) {
            slot = i;
            old = s_subs[i].func;
            break;
        }
        if (s_subs[i].func == NULL && slot < 0) {
            slot = i;
        }
    }
    if (slot >= 0) {
        s_subs[slot].handle = handle;
        s_subs[slot].func = func;
        func = NULL;   // handed to the entry
    }
    xSemaphoreGive(s_subs_lock);

    // Step 4: free outside the lock: the replaced handler, or our copy when no entry was free
    val_free(old);
    if (func != NULL) {
        val_free(func);
        ti_raise_error(handle, TI_ERR_LIMIT_EXCEEDED, "system.subscribe: all %d subscriptions in use", MAX_SUBS);
        return NULL;
    }
    return val_new_void();
}

/**
 * @brief Function system.unsubscribe(): remove the subscription of the calling script.
 * @param handle Calling runtime handle.
 * @param argv Unused.
 * @param argc Unused.
 * @return Void value.
 */
static value_t *system_unsubscribe(ti_handle_t handle, value_t **argv, int argc) {
    (void)argv;
    (void)argc;
    remove_sub(handle);
    return val_new_void();
}

/*---- STATIC FUNCTIONS: DISPATCHER HOOK ----*/

/**
 * @brief Dispatcher hook: forwards a signal to every subscribed script.
 * @param signal Signal that was dispatched.
 * @param sync True when the signal came from dispatch_signal_sync (answer awaited with ti_call).
 */
static void on_signal(Signal_t signal, bool sync) {
    // Step 1: snapshot the list with OWN copies of the handlers, then release the lock BEFORE calling into scripts
    // (a handler may run natives that take other locks, or subscribe / unsubscribe itself and free the entry's value)
    SignalSub_t subs[MAX_SUBS];
    int count = 0;
    xSemaphoreTake(s_subs_lock, portMAX_DELAY);
    for (int i = 0; i < MAX_SUBS; i++) {
        if (s_subs[i].func == NULL) continue;
        value_t *func = val_copy(s_subs[i].func);
        if (func == NULL) continue;   // out of memory: this script misses this signal
        subs[count].handle = s_subs[i].handle;
        subs[count].func = func;
        count++;
    }
    xSemaphoreGive(s_subs_lock);

    for (int i = 0; i < count; i++) {
        // Step 2: call the handler with the signal number as its only argument
        ti_status_t status = TI_ERR_NO_MEMORY;
        value_t *arg = val_new_int((int)signal);
        if (arg != NULL) {
            value_t *args[1] = { arg };
            if (sync) {
                // ti_call BORROWS the arguments: we free arg. The result is ours too.
                value_t *result = NULL;
                status = ti_call(subs[i].handle, subs[i].func, args, 1, &result, SYNC_TIMEOUT_MS);
                val_free(arg);
                val_free(result);
            } else {
                // ti_post_event TAKES the arguments (also on failure): we must not free arg
                status = ti_post_event(subs[i].handle, subs[i].func, args, 1);
            }
        }

        // Step 3: react to the outcome. Errors raised INSIDE the handler are logged by the runtime itself.
        switch (status) {
            case TI_ERR_STALE_HANDLE:
                // The script that registered it is gone: drop the subscription silently
                remove_sub(subs[i].handle);
                break;
            case TI_ERR_UNDEFINED:
            case TI_ERR_INVALID_ARG:
            case TI_ERR_TYPE_MISMATCH:
                // Wrong handler (wrong parameters, or not callable): report and drop it to avoid log spam
                LOG_WARNING_STR("[Dispatcher] Script handler rejected signal %d (status %d), subscription removed",
                                (int)signal, (int)status);
                remove_sub(subs[i].handle);
                break;
            default:
                // TI_OK, timeout, queue full, interrupted...: nothing to do
                break;
        }

        // Step 4: our copy of the handler (borrowed by ti_call / ti_post_event)
        val_free(subs[i].func);
    }
}

/*---- PUBLIC FUNCTIONS ----*/

/**
 * @brief Make the module importable by scripts ('import system;') and forward every dispatched signal to it.
 */
void system_module_init(void) {
    s_subs_lock = xSemaphoreCreateMutex();
    if (s_subs_lock == NULL) {
        LOG_ERROR("system module: cannot create the lock");
        return;
    }
    if (!ti_register_module(&s_system_module)) {
        LOG_ERROR("system module: registration failed");
        return;
    }
    dispatcher_set_hook(on_signal);
}
