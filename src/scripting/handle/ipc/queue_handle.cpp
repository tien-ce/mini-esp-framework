/**
 * @file queue_handle.cpp
 * @brief Script handle "queue" (module ipc): send, recv and destroy of one queue, under the queue's own lock.
 */

/*---- INCLUDES ----*/
#include "scripting/handle/ipc/queue_handle.h"
#include <Arduino.h>
#include <string.h>
#include "core/core_log.h"

/*---- TYPES ----*/

/** @brief A script blocked in recv on a queue (lives on the stack of queue_recv). */
struct ipc_waiter {
    ti_handle_t        runtime;   /**< Runtime of the waiting script, woken with ti_give */
    bool               done;      /**< Flag for ti_wait_flag */
    struct ipc_waiter *next;
};

/*---- STATIC FUNCTION PROTOTYPES ----*/

static void queue_destroy(void *ptr);

static value_t *queue_send(ti_handle_t handle, value_t **argv, int argc);
static value_t *queue_recv(ti_handle_t handle, value_t **argv, int argc);

/*---- PARAMS ----*/

/** send(msg): msg of any type. params[0] is self: the method is found through the handle's type, so it is always a queue. */
static const param_t s_queue_send_params[] = { PARAM_HANDLE("self", NULL), PARAM_ANY("msg"), };
/** recv(timeout_ms). */
static const param_t s_queue_recv_params[] = { PARAM_HANDLE("self", NULL), PARAM_INT("timeout_ms"), };

/*---- METHODS ----*/

static const ti_builtin_def_t s_queue_methods[] = {
    { "send", VAL_BOOL, s_queue_send_params, 2, queue_send },   /* false: queue full */
    { "recv", VAL_ANY,  s_queue_recv_params, 2, queue_recv },   /* the message, or null on timeout */
};

/*---- HANDLE TYPE ----*/

/* External linkage comes from the extern declaration in queue_handle.h */
const ti_handle_type_t g_queue_handle_type = {
    "queue",                                                        /* name */
    queue_destroy,                                                  /* destroy */
    s_queue_methods,                                                /* methods */
    (int)(sizeof(s_queue_methods) / sizeof(s_queue_methods[0])),    /* method_count */
    NULL,                                                           /* get_attr: no attributes */
    NULL,                                                           /* set_attr: no attributes */
};

/*---- STATIC FUNCTIONS: METHODS ----*/

/**
 * @brief Method queue.send(msg): put a copy of msg (any type) at the end of the queue and wake the oldest
 *        script waiting in recv. Never waits: a full queue returns false at once.
 * @param handle Calling runtime handle, used to raise errors.
 * @param argv Arguments: (self, msg). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Bool: true when queued, false when the queue is full; NULL after raising an error.
 */
static value_t *queue_send(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    ipc_queue_t *q = (ipc_queue_t *)argv[0]->handle_val->ptr;

    // Step 1: copy outside the lock: msg belongs to this script, and nobody else knows the copy yet
    value_t *copy = val_deep_copy(argv[1], TI_MAX_CONTAINER_DEPTH);
    if (copy == NULL) {
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "queue.send: out of memory");
        return NULL;
    }

    xSemaphoreTake(q->lock, portMAX_DELAY);

    // Step 2: full queue: drop the copy, the script decides what to do
    if (q->count == q->capacity) {
        xSemaphoreGive(q->lock);
        val_free(copy);
        // name is stable while this handle exists (refs > 0)
        LOG_WARNING_STR("Queue \"%s\" is full (%d messages), message dropped", q->name, q->capacity);
        return val_new_bool(false);
    }

    // Step 3: put the pointer at the end of the ring buffer; from here the queue owns the copy
    q->items[(q->head + q->count) % q->capacity] = copy;
    q->count++;

    // Step 4: first come, first served: take the oldest waiter out of the list and wake it
    //         (out of the list, so the next send wakes the next waiter, not this one again)
    ipc_waiter_t *w = q->waiters;
    if (w != NULL) {
        q->waiters = w->next;
        __atomic_store_n(&w->done, true, __ATOMIC_RELEASE);
        ti_give(w->runtime);
    }

    xSemaphoreGive(q->lock);
    return val_new_bool(true);
}

/**
 * @brief Method queue.recv(int timeout_ms): take the oldest message out of the queue and return it.
 *        Empty queue: wait until a send or timeout_ms (0: do not wait, < 0: wait until a message comes).
 *        While waiting the script stays responsive (events run, tien_stop ends it).
 * @param handle Calling runtime handle.
 * @param argv Arguments: (self, timeout_ms). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return The message (any type, as sent), or null on timeout (or when the script is being stopped).
 */
static value_t *queue_recv(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    ipc_queue_t *q   = (ipc_queue_t *)argv[0]->handle_val->ptr;
    int timeout_ms   = argv[1]->int_val;
    bool forever     = (timeout_ms < 0);
    uint32_t start   = millis();
    ipc_waiter_t w;

    while (true) {
        xSemaphoreTake(q->lock, portMAX_DELAY);

        // Step 1: a message is there: take the oldest one out of the queue and hand it to the script
        //         (the queue owned it; out of the queue, the interpreter owns it)
        if (q->count > 0) {
            value_t *m = q->items[q->head];
            q->items[q->head] = NULL;
            q->head = (q->head + 1) % q->capacity;
            q->count--;
            xSemaphoreGive(q->lock);
            return m;
        }

        // Step 2: empty: give up when the time is over, else compute how long to sleep
        uint32_t elapsed = millis() - start;
        if (!forever && elapsed >= (uint32_t)timeout_ms) {
            xSemaphoreGive(q->lock);
            return val_new_null();
        }
        uint32_t wait_ms = forever ? TI_PORT_WAIT_FOREVER : (uint32_t)timeout_ms - elapsed;

        // Step 3: join the end of the waiter list (first come, first served), then sleep
        //         until send wakes this script or the time is up
        w.runtime = handle;
        w.done    = false;
        w.next    = NULL;
        ipc_waiter_t **tail = &q->waiters;
        while (*tail != NULL) tail = &(*tail)->next;
        *tail = &w;
        xSemaphoreGive(q->lock);

        ti_status_t st = ti_wait_flag(handle, &w.done, wait_ms);

        // Step 4: not woken by send (time up or stopped): leave the list, w lives on this stack
        xSemaphoreTake(q->lock, portMAX_DELAY);
        if (!__atomic_load_n(&w.done, __ATOMIC_ACQUIRE)) {
            ipc_waiter_t **p = &q->waiters;
            while (*p != NULL && *p != &w) p = &(*p)->next;
            if (*p != NULL) *p = w.next;
        }
        xSemaphoreGive(q->lock);

        // Stopped: return at once, the script ends by itself at its next statement
        if (st == TI_ERR_INTERRUPTED || st == TI_ERR_STALE_HANDLE) {
            return val_new_null();
        }
        // Back to step 1: take the message, or wait the remaining time
    }
}

/*---- STATIC FUNCTIONS: DESTRUCTOR ----*/

/**
 * @brief Destructor of the queue handle type: runs once per handle value, when the last reference to it
 *        in its script is freed (out of scope, script ended or stopped). The last handle of all scripts
 *        (refs reaches 0) cleans the queue; the slot is then free for the module (name is kept).
 * @param ptr The ipc_queue_t the handle value points to (never NULL when called).
 */
static void queue_destroy(void *ptr) {
    ipc_queue_t *q = (ipc_queue_t *)ptr;
    value_t *left[IPC_QUEUE_MAX_MSGS];
    int left_count = 0;

    xSemaphoreTake(q->lock, portMAX_DELAY);

    // Step 1: one handle less; other scripts still use the queue
    q->refs--;
    if (q->refs > 0) {
        xSemaphoreGive(q->lock);
        return;
    }

    // Step 2: last handle: take the messages nobody received out of the queue and reset it
    for (int i = 0; i < q->count; i++) {
        int idx = (q->head + i) % q->capacity;
        left[left_count++] = q->items[idx];
        q->items[idx] = NULL;
    }
    q->head    = 0;
    q->count   = 0;
    q->waiters = NULL;

    // Copy the name before unlocking: once refs is 0 the module may reuse the slot and rewrite it
    char name[IPC_QUEUE_NAME_LEN];
    strcpy(name, q->name);

    xSemaphoreGive(q->lock);

    LOG_INFO_STR("Queue \"%s\" is released (%d unreceived messages dropped)", name, left_count);

    // Step 3: free the messages outside the lock: a message may hold a queue handle,
    //         whose destructor (this function) takes a queue lock again
    for (int i = 0; i < left_count; i++) {
        val_free(left[i]);
    }
}
