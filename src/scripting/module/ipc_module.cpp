/**
 * @file ipc_module.cpp
 * @brief Script module "ipc": links between scripts (ipc.QUEUE()). Owns the queue slots.
 */

/*---- INCLUDES ----*/
#include <Arduino.h>
#include <string.h>
#include "TienInterpreter.h"
#include "scripting/handle/ipc/queue_handle.h"
#include "scripting/module/module_manager.h"
#include "core/core_log.h"

/*---- CONFIG ----*/
#define IPC_MAX_QUEUES  8         /**< Queues open at the same time, all scripts together */

/*---- STATIC VARIABLES ----*/

/** @brief Every queue; a slot is in use while its refs > 0. */
static ipc_queue_t s_queues[IPC_MAX_QUEUES];
/** @brief Module lock: makes "find by name, else create" one step (never used by the queue handle). */
static SemaphoreHandle_t s_ipc_lock = NULL;

/*---- STATIC FUNCTION PROTOTYPES ----*/

static value_t *ipc_queue(ti_handle_t handle, value_t **argv, int argc);
static ipc_queue_t *queue_find(const char *name);
static ipc_queue_t *queue_create(const char *name, int capacity);

/*---- PARAMS ----*/

/** QUEUE(name, capacity). */
static const param_t s_ipc_queue_params[] = { PARAM_STRING("name"), PARAM_INT("capacity"), };

/*---- FUNCTIONS ----*/

static const ti_builtin_def_t s_ipc_funcs[] = {
    { "QUEUE", VAL_HANDLE, s_ipc_queue_params, 2, ipc_queue },
};

/*---- MODULE ----*/

static const ti_module_t s_ipc_module = {
    "ipc",                                                          /* name */
    s_ipc_funcs,                                                    /* funcs */
    (int)(sizeof(s_ipc_funcs) / sizeof(s_ipc_funcs[0])),            /* func_count */
    NULL,                                                           /* consts: none */
    0                                                               /* const_count */
};

/*---- STATIC FUNCTIONS ----*/

/**
 * @brief Find the live queue called name and count one more handle on it. Module lock held by the caller.
 * @param name Queue name.
 * @return The queue (refs already incremented), or NULL when no live queue has this name.
 */
static ipc_queue_t *queue_find(const char *name) {
    for (int i = 0; i < IPC_MAX_QUEUES; i++) {
        ipc_queue_t *q = &s_queues[i];

        // Different name: skip without the queue lock (name is only written here, under the module lock)
        if (strcmp(q->name, name) != 0) continue;

        // Same name: check under the queue lock that it is still alive (destroy may have brought refs to 0)
        xSemaphoreTake(q->lock, portMAX_DELAY);
        if (q->refs > 0) {
            q->refs++;
            xSemaphoreGive(q->lock);
            return q;
        }
        xSemaphoreGive(q->lock);
    }
    return NULL;
}

/**
 * @brief Create a queue called name in a free slot (refs == 0), with refs = 1. Module lock held by the caller.
 * @param name Queue name.
 * @param capacity Max messages.
 * @return The queue, or NULL when every slot is in use.
 */
static ipc_queue_t *queue_create(const char *name, int capacity) {
    for (int i = 0; i < IPC_MAX_QUEUES; i++) {
        ipc_queue_t *q = &s_queues[i];

        // Take the queue lock: a destroy still cleaning this slot finishes first
        xSemaphoreTake(q->lock, portMAX_DELAY);
        if (q->refs == 0) {
            strcpy(q->name, name);
            q->capacity = capacity;
            q->head     = 0;
            q->count    = 0;
            q->waiters  = NULL;
            q->refs     = 1;
            xSemaphoreGive(q->lock);
            return q;
        }
        xSemaphoreGive(q->lock);
    }
    return NULL;
}

/**
 * @brief ipc.QUEUE(string name, int capacity): open the queue called name, created on the first call.
 *        Every call returns a new handle value; handles opened with the same name share one queue.
 * @param handle Calling runtime handle, used to raise errors.
 * @param argv Arguments: (name, capacity). capacity is used only when the queue is created.
 * @param argc Argument count (unused).
 * @return Handle value of type "queue"; NULL after raising an error.
 */
static value_t *ipc_queue(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    const char *name = argv[0]->string_val;
    int capacity     = argv[1]->int_val;

    if (name[0] == '\0' || strlen(name) >= IPC_QUEUE_NAME_LEN) {
        ti_raise_error(handle, TI_ERR_INVALID_ARG, "ipc.QUEUE: name must be 1..%d characters", IPC_QUEUE_NAME_LEN - 1);
        return NULL;
    }
    if (capacity < 1 || capacity > IPC_QUEUE_MAX_MSGS) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "ipc.QUEUE: capacity must be 1..%d", IPC_QUEUE_MAX_MSGS);
        return NULL;
    }

    // Step 1: find or create as one step under the module lock (refs++ for this handle)
    xSemaphoreTake(s_ipc_lock, portMAX_DELAY);
    ipc_queue_t *q = queue_find(name);
    if (q == NULL) q = queue_create(name, capacity);
    xSemaphoreGive(s_ipc_lock);
    if (q == NULL) {
        LOG_WARNING_STR("Cannot create queue \"%s\": all %d queue slots in use", name, IPC_MAX_QUEUES);
        ti_raise_error(handle, TI_ERR_LIMIT_EXCEEDED, "ipc.QUEUE: all %d queues in use", IPC_MAX_QUEUES);
        return NULL;
    }

    // Step 2: a new handle value for this script, pointing to the shared queue
    value_t *v = val_new_handle(q, &g_queue_handle_type);
    if (v == NULL) {
        g_queue_handle_type.destroy(q);   // give back the refs++ of step 1
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "ipc.QUEUE: out of memory");
        return NULL;
    }
    return v;
}

/*---- PUBLIC FUNCTIONS ----*/

/**
 * @brief Create the module lock and the lock of every queue slot, then make the module importable
 *        by scripts ('import ipc;').
 */
void ipc_module_init(void) {
    s_ipc_lock = xSemaphoreCreateMutex();
    if (s_ipc_lock == NULL) {
        LOG_ERROR("ipc module: cannot create the module lock");
        return;
    }
    for (int i = 0; i < IPC_MAX_QUEUES; i++) {
        s_queues[i].lock = xSemaphoreCreateMutex();
        if (s_queues[i].lock == NULL) {
            LOG_ERROR("ipc module: cannot create a queue lock");
            return;
        }
    }
    if (!ti_register_module(&s_ipc_module)) {
        LOG_ERROR("ipc module: registration failed");
    }
}
