#include <Arduino.h>
#include "built_in.h"
#include "TienInterpreter.h"
#include "core/dispatcher.h"
#include "core/core_log.h"
#include <string.h>
#include <vector>

/*
 * CENTRAL EVENT DISPATCHER FOR SCRIPTS
 *
 *   dispatcher_register(func handler)   subscribe the calling script to EVERY firmware signal
 *   dispatcher_unregister()             remove the calling script's subscription
 *
 * Each script (runtime handle) keeps at most ONE handler; registering again replaces it. The handler is called for every signal
 * with ONE argument: the signal number (VAL_INT), and decides itself what to do with it (if sig == ... else if ...).
 *   - Signals dispatched with dispatch_signal_sync() (SIG_WEB_POLL, SIG_MQTT_PUBLISH) call the handler with ti_call:
 *     the dispatching task waits until the handler returned.
 *   - Every other signal is posted with ti_post_event(): fire and forget, queued in order.
 *
 * VALUE OWNERSHIP (who frees what):
 *   - argv of the builtins: owned by the interpreter. We only READ it and never keep a pointer to it
 *     (the handler name is copied into the subscriber list).
 *   - Argument we pass to ti_call: BORROWED by ti_call, so we create it and we free it. The result is ours too.
 *   - Argument we pass to ti_post_event: ownership ALWAYS moves to ti_post_event (even when it fails), we must not free it.
 * See SCRIPT_DISPATCHER.md for which signals are meant to be captured.
 */

/* -------------------------------------------------------------------------- */
/*                                  CONSTANTS                                 */
/* -------------------------------------------------------------------------- */

#define SUB_NAME_MAX       32     /* Longest handler function name (including the terminator) */
#define SYNC_TIMEOUT_MS    200    /* How long a sync dispatch waits for the script to reach a safe point */

/* -------------------------------------------------------------------------- */
/*                                    TYPES                                   */
/* -------------------------------------------------------------------------- */

/** One subscribed script. */
struct SignalSub {
    ti_handle_t handle;                /* Runtime that registered the handler */
    char        name[SUB_NAME_MAX];    /* Handler function name (copy) */
};

/* -------------------------------------------------------------------------- */
/*                              STATIC VARIABLES                              */
/* -------------------------------------------------------------------------- */

static std::vector<SignalSub> s_subs;   /* Subscribers, guarded by subMutex() */

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Returns the mutex that guards the list (written by script threads, read by dispatching tasks).
 * @return Mutex handle.
 */
static SemaphoreHandle_t subMutex() {
    static SemaphoreHandle_t m = xSemaphoreCreateMutex();
    return m;
}

/**
 * @brief Removes the subscription of a runtime (no-op when it has none).
 * @param handle Runtime whose subscription is removed.
 */
static void removeSub(ti_handle_t handle) {
    xSemaphoreTake(subMutex(), portMAX_DELAY);
    for (size_t i = 0; i < s_subs.size(); i++) {
        if (s_subs[i].handle == handle) {
            s_subs.erase(s_subs.begin() + i);
            break;
        }
    }
    xSemaphoreGive(subMutex());
}

/**
 * @brief Built-in dispatcher_register(func handler): subscribes the calling script to all signals.
 * @param handle Calling runtime handle (stored so the dispatcher can reach the script later).
 * @param argv Arguments: (func handler). Owned by the interpreter, never kept.
 * @param argc Argument count.
 * @return Null value, or NULL after raising an error.
 */
static value_t *builtin_dispatcher_register(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;

    // Step 1: validate the handler name (it must fit in the entry, the value itself is only borrowed)
    const char *funcName = argv[0]->func_name;
    if (funcName == NULL || funcName[0] == '\0' || strlen(funcName) >= SUB_NAME_MAX) {
        ti_raise_error(handle, TI_ERR_INVALID_ARG, "dispatcher_register() expects a handler name of 1..%d characters", SUB_NAME_MAX - 1);
        return NULL;
    }

    // Step 2: replace the runtime's previous entry or append a new one (a COPY of the name is stored)
    xSemaphoreTake(subMutex(), portMAX_DELAY);
    SignalSub *entry = NULL;
    for (size_t i = 0; i < s_subs.size(); i++) {
        if (s_subs[i].handle == handle) { entry = &s_subs[i]; break; }
    }
    if (entry == NULL) {
        SignalSub fresh;
        fresh.handle = handle;
        s_subs.push_back(fresh);
        entry = &s_subs.back();
    }
    strncpy(entry->name, funcName, SUB_NAME_MAX - 1);
    entry->name[SUB_NAME_MAX - 1] = '\0';
    xSemaphoreGive(subMutex());
    return val_new_null();
}

/**
 * @brief Built-in dispatcher_unregister(): removes the subscription of the calling script.
 * @param handle Calling runtime handle.
 * @param argv Unused.
 * @param argc Unused.
 * @return Null value.
 */
static value_t *builtin_dispatcher_unregister(ti_handle_t handle, value_t **argv, int argc) {
    (void)argv;
    (void)argc;
    removeSub(handle);
    return val_new_null();
}

/**
 * @brief Dispatcher hook: forwards a signal to every subscribed script.
 * @param signal Signal that was dispatched.
 * @param sync True when the signal came from dispatch_signal_sync (answer awaited with ti_call).
 */
static void onSignal(Signal_t signal, bool sync) {
    // Step 1: snapshot the list and release the mutex BEFORE calling into scripts
    // (a handler may run natives that take other locks, or register/unregister itself)
    xSemaphoreTake(subMutex(), portMAX_DELAY);
    std::vector<SignalSub> subs = s_subs;
    xSemaphoreGive(subMutex());

    for (const SignalSub &sub : subs) {
        // Step 2: call the handler with the signal number as its only argument
        ti_status_t status;
        if (sync) {
            // ti_call BORROWS the arguments: we create the value and we free it. The result is ours too.
            value_t *arg = val_new_int((int)signal);
            if (arg == NULL) continue;
            value_t *args[1] = { arg };
            value_t *result = NULL;
            status = ti_call(sub.handle, sub.name, args, 1, &result, SYNC_TIMEOUT_MS);
            val_free(arg);
            val_free(result);
        } else {
            // ti_post_event TAKES the arguments (also on failure): we must not free them
            value_t *arg = val_new_int((int)signal);
            if (arg == NULL) continue;
            value_t *args[1] = { arg };
            status = ti_post_event(sub.handle, sub.name, args, 1);
        }

        // Step 3: react to the outcome. Errors raised INSIDE the handler are logged by the runtime itself.
        switch (status) {
            case TI_ERR_STALE_HANDLE:
                // The script that registered it is gone: drop the subscription silently
                removeSub(sub.handle);
                break;
            case TI_ERR_UNDEFINED:
            case TI_ERR_INVALID_ARG:
            case TI_ERR_TYPE_MISMATCH:
                // Wrong handler (missing function or wrong parameters): report and drop it to avoid log spam
                LOG_WARNING_STR("[Dispatcher] Script handler '%s' rejected (status %d), subscription removed",
                                sub.name, (int)status);
                removeSub(sub.handle);
                break;
            default:
                // TI_OK, timeout, queue full, interrupted...: nothing to do
                break;
        }
    }
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

void builtin_dispatcher_init(void) {
    static param_t reg_params[] = { { VAL_FUNC, (char *)"handler", VAL_NULL } };

    register_builtin_function("dispatcher_register", VAL_VOID, reg_params, 1, builtin_dispatcher_register);
    register_builtin_function("dispatcher_unregister", VAL_VOID, NULL, 0, builtin_dispatcher_unregister);

    // Forward every dispatched signal to the subscribed scripts
    dispatcher_set_hook(onSignal);
}
