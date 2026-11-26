#ifndef QUEUE_HANDLE_H
#define QUEUE_HANDLE_H

/**
 * @file queue_handle.h
 * @brief Script handle "queue" (module ipc): one named message queue shared by scripts.
 *
 * Roles:
 *   - module ipc  : owns the slots (array of ipc_queue_t), finds or creates a queue by name in ipc.QUEUE(),
 *                   writes `name` (only there, under the module lock) and does the refs++ of each new handle.
 *   - queue handle: everything after that: send, recv, and destroy (refs--, cleans the queue at 0).
 *                   Uses only the queue's own lock, never the module lock.
 * A slot is in use while refs > 0; destroy never clears `name`, so the module can compare names
 * without taking the queue lock.
 */

/*---- INCLUDES ----*/
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "TienInterpreter.h"

/*---- CONFIG ----*/
#define IPC_QUEUE_MAX_MSGS  16    /**< Upper limit of a queue's capacity */
#define IPC_QUEUE_NAME_LEN  32    /**< Name buffer, '\0' included */

/*---- TYPES ----*/

/** @brief A script blocked in recv (defined in queue_handle.cpp, only the handle uses it). */
typedef struct ipc_waiter ipc_waiter_t;

/** @brief A named queue: ring buffer of messages shared by every handle opened with its name. */
typedef struct {
    char              name[IPC_QUEUE_NAME_LEN];   /**< Written only by the module when it creates the queue */
    SemaphoreHandle_t lock;                       /**< Guards every field below (created by the module at init) */
    int               refs;                       /**< Handle values pointing to this queue, all scripts; 0 = free slot */
    int               capacity;                   /**< Max messages (<= IPC_QUEUE_MAX_MSGS) */
    int               head;                       /**< Index of the oldest message */
    int               count;                      /**< Messages in the queue */
    value_t          *items[IPC_QUEUE_MAX_MSGS];  /**< Messages: copies owned by the queue */
    ipc_waiter_t     *waiters;                    /**< Scripts blocked in recv, oldest first (NULL: none) */
} ipc_queue_t;

/*---- HANDLE TYPE ----*/

/**
 * @brief Handle type of a queue value: name "queue", methods send(msg) -> bool, recv(timeout_ms) -> msg or null.
 * @details Wrap a queue with val_new_handle(q, &g_queue_handle_type) after its refs++ (done by the module).
 *          The destructor does refs-- and cleans the queue when it was the last handle.
 */
extern const ti_handle_type_t g_queue_handle_type;

#endif // QUEUE_HANDLE_H
