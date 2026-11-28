/**
 * @file driver_shared.cpp
 * @brief Resources shared by several drivers (see driver_shared.h).
 */

/*---- INCLUDES ----*/
#include <HardwareSerial.h>
#include <driver/uart.h>        /* uart_is_driver_installed */
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "hardware/driver/driver_shared.h"

/*---- STATIC VARIABLES ----*/

static StaticSemaphore_t s_uart_lock_buffer;   /* Memory of the UART lock: static, no heap */
static SemaphoreHandle_t s_uart_lock = NULL;   /* Set by driver_shared_create_locks() before setup() */

/* Hardware UARTs the drivers may take: s_hardware_uarts[i] is UART i + 1 (UART0 is the console) */
static HardwareSerial *const s_hardware_uarts[] = { &Serial1, &Serial2 };

/*---- STATIC FUNCTIONS ----*/

/**
 * @brief Create the locks of this file. Runs once at startup, before setup().
 * @details GCC puts its address in .init_array, which ESP-IDF runs before app_main (and before the scheduler
 *          starts: creating a mutex there is allowed). Runs too early to log. Never fails: the memory is static.
 */
__attribute__((constructor))
static void driver_shared_create_locks(void) {
    s_uart_lock = xSemaphoreCreateMutexStatic(&s_uart_lock_buffer);
}

/*---- PUBLIC FUNCTIONS ----*/

void DriverShared_UartLock(void) {
    xSemaphoreTake(s_uart_lock, portMAX_DELAY);
}

void DriverShared_UartUnlock(void) {
    xSemaphoreGive(s_uart_lock);
}

HardwareSerial *DriverShared_FindFreeUart(void) {
    for (uint8_t i = 0; i < sizeof(s_hardware_uarts) / sizeof(s_hardware_uarts[0]); i++) {
        if (!uart_is_driver_installed((uart_port_t)(i + 1))) {
            return s_hardware_uarts[i];
        }
    }
    return NULL;
}
