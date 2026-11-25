#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <stdbool.h>
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define NUM_DRIVERS 10
#define NUM_SENSORS 10
#define NUM_ADAPTERS 10

// Macro to declare a single weak Xdrv function returning false
#define DEFINE_WEAK_XDRV(num) \
    __attribute__((weak)) bool Xdrv##num(Signal_t func) { return false; }

// Macro to declare all weak Xsns functions from 1 to NUM_SENSORS
#define DEFINE_WEAK_XSNS(num) \
    __attribute__((weak)) bool Xsns##num(Signal_t func) { return false; }

// Macro to declare all weak Xadp functions from 1 to NUM_ADAPTERS
#define DEFINE_WEAK_XADP(num) \
    __attribute__((weak)) bool Xadp##num(Signal_t func) { return false; }

typedef enum {
    SIG_INIT = 0,
    /* Timer */
    SIG_10MS,
    SIG_100MS,
    SIG_1SEC,
    
    /* Wifi */ 
    SIG_WIFI_CONNECTED,
    SIG_WIFI_DISCONNECTED,

    /* Web */
    SIG_WEB_POLL,
    
    /* MQTT */
    SIG_MQTT_PUBLISH,
    SIG_MAX

} Signal_t;

/** @brief Asynchronous dispatch: notifies the dispatcher task and returns immediately (signals may be merged). */
void dispatch_signal(Signal_t signal);

/**
 * @brief Synchronous dispatch: runs every module's handler for the signal in the CALLER's task and returns when all are done.
 * @details No lock is taken: each module is responsible for the thread safety of its own handler. Handlers must only copy
 *          data (no delay, no slow I/O). Does nothing until the dispatcher has finished SIG_INIT for all modules.
 * @param signal Signal to dispatch.
 */
void dispatch_signal_sync(Signal_t signal);

/** Hook called after the module handlers of a signal ran (used to forward the signal to script handlers). */
typedef void (*DispatchHook_t)(Signal_t signal, bool sync);

/**
 * @brief Installs the hook that is called for every dispatched signal after the Xdrv/Xsns/Xadp handlers.
 * @param hook Function to call (sync is true when the signal came from dispatch_signal_sync), or nullptr to remove it.
 */
void dispatcher_set_hook(DispatchHook_t hook);

/** @brief Initialize hardware timer interrupt for signal generation */
void dispatcher_init(void);

/** @brief Trigger signal from ISR context */
void dispatch_signal_from_isr(Signal_t signal, BaseType_t *pxHigherPriorityTaskWoken);

#endif // DISPATCHER_H
