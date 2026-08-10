#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <stdbool.h>
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define NUM_DRIVERS 10
#define NUM_SENSORS 10
// Macro to declare a single weak Xdrv function returning false
#define DEFINE_WEAK_XDRV(num) \
    __attribute__((weak)) bool Xdrv##num(Signal_t func) { return false; }

// Macro to declare all weak Xsns functions from 1 to NUM_SENSORS
#define DEFINE_WEAK_XSNS(num) \
    __attribute__((weak)) bool Xsns##num(Signal_t func) { return false; }

typedef enum {
    SIG_INIT = 0,
    /* Timer */
    SIG_10MS,
    SIG_100MS,
    SIG_1SEC,
    
    /* Web */
    SIG_WEB_POLL,
    
    /* MQTT */
    SIG_MQTT_PUBLISH,
    SIG_MAX

} Signal_t;

void dispatch_signal(Signal_t signal);

/** @brief Initialize hardware timer interrupt for signal generation */
void dispatcher_init(void);

/** @brief Trigger signal from ISR context */
void dispatch_signal_from_isr(Signal_t signal, BaseType_t *pxHigherPriorityTaskWoken);

#endif // DISPATCHER_H
