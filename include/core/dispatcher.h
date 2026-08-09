#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <stdbool.h>
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define NUM_DRIVERS 10

// Macro to declare a single weak Xdrv function returning false
#define DEFINE_WEAK_XDRV(num) \
    __attribute__((weak)) bool Xdrv##num(Signal_t func) { return false; }

typedef enum {
    SIG_10MS = 0,
    SIG_100MS,
    SIG_1SEC,    
    SIG_MAX
} Signal_t;

#define SIG_MASK_10MS  (1UL << SIG_10MS)
#define SIG_MASK_100MS (1UL << SIG_100MS)
#define SIG_MASK_1SEC  (1UL << SIG_1SEC)

/** @brief Dispatch signal to all registered drivers */
void dispatch_signal(Signal_t signal);

/** @brief Initialize hardware timer interrupt for signal generation */
void dispatcher_init(void);

/** @brief Trigger signal from ISR context */
void dispatch_signal_from_isr(Signal_t signal, BaseType_t *pxHigherPriorityTaskWoken);

#endif // DISPATCHER_H
