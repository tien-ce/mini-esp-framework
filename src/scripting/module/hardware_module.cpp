/**
 * @file hardware_module.cpp
 * @brief Script module "hardware": hardware resources for scripts (hardware.UART(), hardware.SERIAL_8N1, ...).
 *
 * Two families of frame constants:
 *  - SERIAL_xxx: Arduino frame constant (enum SerialConfig in HardwareSerial.h), passed to HardwareSerial::begin() as is.
 *  - RTU_xxx:    ModbusSerialConfig_t of the Modbus library (lib/Modbus_arduinoIDE/src/port.h), for Modbus RTU.
 *
 * Written in C++ because SerialConfig is only visible to C++ (Arduino.h includes HardwareSerial.h under __cplusplus).
 * Rows use TI_CONST_INT of the TI library (designated initializers, accepted by g++ as an extension).
 */

/*---- INCLUDES ----*/
#include <Arduino.h>
#include "TienInterpreter.h"
#include "port.h"
#include "scripting/handle/uart_handle.h"
#include "scripting/module/module_manager.h"
#include "core/core_log.h"

/*---- STATIC FUNCTION PROTOTYPES ----*/

static value_t *hardware_uart(ti_handle_t handle, value_t **argv, int argc);

/*---- FUNCTIONS ----*/

static const ti_builtin_def_t s_hardware_funcs[] = {
    { "UART", VAL_HANDLE, NULL, 0, hardware_uart },
};

/*---- CONSTANTS ----*/

static const ti_module_const_t s_hardware_consts[] = {
    /* Arduino frame: <data bits><parity><stop bits> */
    TI_CONST_INT("SERIAL_8N1", SERIAL_8N1),
    TI_CONST_INT("SERIAL_8N2", SERIAL_8N2),
    TI_CONST_INT("SERIAL_8E1", SERIAL_8E1),
    TI_CONST_INT("SERIAL_8E2", SERIAL_8E2),
    TI_CONST_INT("SERIAL_8O1", SERIAL_8O1),
    TI_CONST_INT("SERIAL_8O2", SERIAL_8O2),
    TI_CONST_INT("SERIAL_7N1", SERIAL_7N1),
    TI_CONST_INT("SERIAL_7N2", SERIAL_7N2),
    TI_CONST_INT("SERIAL_7E1", SERIAL_7E1),
    TI_CONST_INT("SERIAL_7E2", SERIAL_7E2),
    TI_CONST_INT("SERIAL_7O1", SERIAL_7O1),
    TI_CONST_INT("SERIAL_7O2", SERIAL_7O2),

    /* Modbus RTU frame (ModbusSerialConfig_t) */
    TI_CONST_INT("RTU_8N1", RTU_8N1),
    TI_CONST_INT("RTU_8N2", RTU_8N2),
    TI_CONST_INT("RTU_8E1", RTU_8E1),
    TI_CONST_INT("RTU_8E2", RTU_8E2),
    TI_CONST_INT("RTU_8O1", RTU_8O1),
    TI_CONST_INT("RTU_8O2", RTU_8O2),
    TI_CONST_INT("RTU_7N1", RTU_7N1),
    TI_CONST_INT("RTU_7N2", RTU_7N2),
    TI_CONST_INT("RTU_7E1", RTU_7E1),
    TI_CONST_INT("RTU_7E2", RTU_7E2),
    TI_CONST_INT("RTU_7O1", RTU_7O1),
    TI_CONST_INT("RTU_7O2", RTU_7O2),
};

/*---- MODULE ----*/

/** The "hardware" module. */
static const ti_module_t s_hardware_module = {
    "hardware",                                                     /* name */
    s_hardware_funcs,                                               /* funcs */
    (int)(sizeof(s_hardware_funcs) / sizeof(s_hardware_funcs[0])),  /* func_count */
    s_hardware_consts,                                              /* consts */
    (int)(sizeof(s_hardware_consts) / sizeof(s_hardware_consts[0])) /* const_count */
};

/*---- STATIC FUNCTIONS ----*/

/**
 * @brief Function hardware.UART(): create a closed uart. The port is chosen later with uart.open().
 * @param handle Calling runtime handle, used to raise errors.
 * @param argv Arguments (none).
 * @param argc Argument count (unused).
 * @return New uart handle value (owns the uart_handle_t); NULL after raising an error.
 */
static value_t *hardware_uart(ti_handle_t handle, value_t **argv, int argc) {
    (void)argv;
    (void)argc;

    // Step 1: the native object, zeroed so port / baud / frame read 0 before open()
    uart_handle_t *self = (uart_handle_t *)calloc(1, sizeof(uart_handle_t));
    if (self == NULL) {
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "hardware.UART: out of memory");
        return NULL;
    }

    // Step 2: wrap it; on success the value owns self (uart_destroy frees it)
    value_t *v = val_new_handle(self, &g_uart_handle_type);
    if (v == NULL) {
        free(self);   // not taken by val_new_handle on failure
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "hardware.UART: out of memory");
        return NULL;
    }
    return v;
}

/*---- PUBLIC FUNCTIONS ----*/

/**
 * @brief Make the module importable by scripts ('import hardware;').
 */
void hardware_module_init(void) {
    if (!ti_register_module(&s_hardware_module)) {
        LOG_ERROR("hardware module: registration failed");
    }
}
