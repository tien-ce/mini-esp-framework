#include "core/dispatcher.h"
#include <stdbool.h>
#include <stdint.h>
#include <climits>
#include <cstddef>
#include <esp_timer.h>

DEFINE_WEAK_XDRV(1)
DEFINE_WEAK_XDRV(2)
DEFINE_WEAK_XDRV(3)
DEFINE_WEAK_XDRV(4)
DEFINE_WEAK_XDRV(5)
DEFINE_WEAK_XDRV(6)
DEFINE_WEAK_XDRV(7)
DEFINE_WEAK_XDRV(8)
DEFINE_WEAK_XDRV(9)
DEFINE_WEAK_XDRV(10)

typedef bool (*XdrvFunc_t)(Signal_t);

/* ----------------------- Static Variables ------------------------------*/
static const XdrvFunc_t g_xdrv_table[NUM_DRIVERS] = {
    Xdrv1, Xdrv2, Xdrv3, Xdrv4, Xdrv5,
    Xdrv6, Xdrv7, Xdrv8, Xdrv9, Xdrv10
};

static TaskHandle_t s_dispatcher_task_handle = NULL;
static volatile uint8_t s_10ms_counter = 0;
static volatile uint8_t s_100ms_counter = 0;

/* ----------------------- Static functions -----------------------------*/

/**
 * @brief ESP Timer ISR Callback (runs every 10ms)
 */
static void IRAM_ATTR on_dispatcher_timer(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // 1. Send 10ms signal on every interrupt
    dispatch_signal_from_isr(SIG_10MS, &xHigherPriorityTaskWoken);

    // 2. Count 10 x 10ms = 100ms
    s_10ms_counter++;
    if (s_10ms_counter >= 10) {
        s_10ms_counter = 0;
        dispatch_signal_from_isr(SIG_100MS, &xHigherPriorityTaskWoken);

        // 3. Count 10 x 100ms = 1s
        s_100ms_counter++;
        if (s_100ms_counter >= 10) {
            s_100ms_counter = 0;
            dispatch_signal_from_isr(SIG_1SEC, &xHigherPriorityTaskWoken);
        }
    }

    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief Internal function to execute drivers for a given signal
 */
static void execute_dispatch(Signal_t signal) {
    for (uint8_t i = 0; i < NUM_DRIVERS; i++) {
        if (g_xdrv_table[i] != NULL) {
            g_xdrv_table[i](signal);
        }
    }
}

/**
 * @brief Dispatcher Task: Sleeps until notified by ISR or non-ISR calls
 */
static void vDispatcherTask(void *pvParameters) {
    uint32_t notified_bits = 0;

    for (;;) {
        if (xTaskNotifyWait(0x00, ULONG_MAX, &notified_bits, portMAX_DELAY) == pdTRUE) {
            if (notified_bits & SIG_MASK_10MS) {
                execute_dispatch(SIG_10MS);
            }
            if (notified_bits & SIG_MASK_100MS) {
                execute_dispatch(SIG_100MS);
            }
            if (notified_bits & SIG_MASK_1SEC) {
                execute_dispatch(SIG_1SEC);
            }
        }
    }
}

/* -------------- Public functions --------------------------*/

void dispatcher_init(void) {
    xTaskCreatePinnedToCore(
        vDispatcherTask,
        "DispatcherTask",
        4096,
        NULL,
        2,
        &s_dispatcher_task_handle,
        1
    );

    const esp_timer_create_args_t timer_args = {
        .callback = &on_dispatcher_timer,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "dispatcher_timer"
    };

    esp_timer_handle_t timer_handle;
    esp_timer_create(&timer_args, &timer_handle);
    esp_timer_start_periodic(timer_handle, 10000); // 10,000 us = 10 ms
}

void dispatch_signal(Signal_t signal) {
    if (s_dispatcher_task_handle == NULL || signal >= SIG_MAX) return;

    uint32_t bit_to_set = (1UL << signal);
    xTaskNotify(s_dispatcher_task_handle, bit_to_set, eSetBits);
}

void dispatch_signal_from_isr(Signal_t signal, BaseType_t *pxHigherPriorityTaskWoken) {
    if (s_dispatcher_task_handle == NULL || signal >= SIG_MAX) return;

    uint32_t bit_to_set = (1UL << signal);
    xTaskNotifyFromISR(s_dispatcher_task_handle, bit_to_set, eSetBits, pxHigherPriorityTaskWoken);
}
