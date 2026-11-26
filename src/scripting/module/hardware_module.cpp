/**
 * @file hardware_module.cpp
 * @brief Script module "hardware": hardware resources for scripts (hardware.UART(), hardware.MODBUS(),
 *        hardware.relay_get(), hardware.relay_set(), hardware.relay_toggle(), hardware.SERIAL_8N1, ...).
 *        MODBUS() exists only when USE_MODBUS_MASTER is set, relay_xxx only when USE_RELAY is set.
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
#include "config.h"
#include "TienInterpreter.h"
#include "port.h"
#include "scripting/handle/hardware/uart_handle.h"
#ifdef USE_MODBUS_MASTER
#include "scripting/handle/hardware/modbus_handle.h"
#endif
#ifdef USE_RELAY
#include "hardware/driver/xdrv_02_relay.h"
#endif
#include "scripting/module/module_manager.h"
#include "core/core_log.h"

/*---- STATIC FUNCTION PROTOTYPES ----*/

static value_t *hardware_uart(ti_handle_t handle, value_t **argv, int argc);
#ifdef USE_MODBUS_MASTER
static value_t *hardware_modbus(ti_handle_t handle, value_t **argv, int argc);
#endif
#ifdef USE_RELAY
static value_t *hardware_relay_get(ti_handle_t handle, value_t **argv, int argc);
static value_t *hardware_relay_set(ti_handle_t handle, value_t **argv, int argc);
static value_t *hardware_relay_toggle(ti_handle_t handle, value_t **argv, int argc);
#endif

/*---- PARAMS ----*/

#ifdef USE_RELAY
/** relay_get(relay), relay_toggle(relay). */
static const param_t s_relay_get_params[] = { PARAM_INT("relay"), };
/** relay_set(relay, state). state is int, bool or "ON" / "OFF", checked inside the function (VAL_ANY). */
static const param_t s_relay_set_params[] = { PARAM_INT("relay"), PARAM_ANY("state"), };
#endif

/*---- FUNCTIONS ----*/

static const ti_builtin_def_t s_hardware_funcs[] = {
    { "UART",         VAL_HANDLE, NULL,               0, hardware_uart         },
#ifdef USE_MODBUS_MASTER
    { "MODBUS",       VAL_HANDLE, NULL,               0, hardware_modbus       },
#endif
#ifdef USE_RELAY
    { "relay_get",    VAL_BOOL,   s_relay_get_params, 1, hardware_relay_get    },
    { "relay_set",    VAL_VOID,   s_relay_set_params, 2, hardware_relay_set    },
    { "relay_toggle", VAL_VOID,   s_relay_get_params, 1, hardware_relay_toggle },
#endif
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

#ifdef USE_MODBUS_MASTER
/**
 * @brief Function hardware.MODBUS(): create a closed Modbus RTU master. The port is chosen later with modbus.open().
 * @param handle Calling runtime handle, used to raise errors.
 * @param argv Arguments (none).
 * @param argc Argument count (unused).
 * @return New modbus handle value (owns the modbus_handle_t); NULL after raising an error.
 */
static value_t *hardware_modbus(ti_handle_t handle, value_t **argv, int argc) {
    (void)argv;
    (void)argc;

    // Step 1: the native object, zeroed so port / baud read 0 before open()
    modbus_handle_t *self = (modbus_handle_t *)calloc(1, sizeof(modbus_handle_t));
    if (self == NULL) {
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "hardware.MODBUS: out of memory");
        return NULL;
    }

    // Step 2: wrap it; on success the value owns self (modbus_destroy frees it)
    value_t *v = val_new_handle(self, &g_modbus_handle_type);
    if (v == NULL) {
        free(self);   // not taken by val_new_handle on failure
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "hardware.MODBUS: out of memory");
        return NULL;
    }
    return v;
}
#endif // USE_MODBUS_MASTER

#ifdef USE_RELAY
/**
 * @brief Function hardware.relay_get(int relay): read whether a relay is on.
 * @param handle Calling runtime handle, used to raise errors.
 * @param argv Arguments: (relay), 1..MAX_RELAYS. Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Bool value, true when the relay is ON; NULL after raising an error.
 */
static value_t *hardware_relay_get(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    int relay = argv[0]->int_val;

    // Step 1: the uint8_t cast would wrap
    if (relay < 1 || relay > MAX_RELAYS) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "hardware.relay_get: relay must be 1..%d, got %d", MAX_RELAYS, relay);
        return NULL;
    }

    // Step 2: read through the driver (init and pin are checked there)
    bool on = false;
    RelayStatus_t status = Relay_GetState((uint8_t)relay, &on);
    if (status != RELAY_OK) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "hardware.relay_get: relay %d: %s", relay, Relay_StatusToStr(status));
        return NULL;
    }
    return val_new_bool(on);
}

/**
 * @brief Function hardware.relay_set(int relay, state): switch a relay on or off.
 * @details state: int (0 = OFF, any other = ON), bool, or the string "ON" / "OFF". Anything else raises.
 * @param handle Calling runtime handle, used to raise errors.
 * @param argv Arguments: (relay, state). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Void value; NULL after raising an error.
 */
static value_t *hardware_relay_set(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    int relay = argv[0]->int_val;
    const value_t *state = argv[1];

    // Step 1: the uint8_t cast would wrap
    if (relay < 1 || relay > MAX_RELAYS) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "hardware.relay_set: relay must be 1..%d, got %d", MAX_RELAYS, relay);
        return NULL;
    }

    // Step 2: VAL_ANY accepts every type, so only int, bool and "ON" / "OFF" are taken here
    bool on;
    if (state->type == VAL_INT) {
        on = (state->int_val != 0);
    } else if (state->type == VAL_BOOL) {
        on = state->bool_val;
    } else if (state->type == VAL_STRING && strcmp(state->string_val, "ON") == 0) {
        on = true;
    } else if (state->type == VAL_STRING && strcmp(state->string_val, "OFF") == 0) {
        on = false;
    } else {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "hardware.relay_set: state must be int, bool, \"ON\" or \"OFF\"");
        return NULL;
    }

    // Step 3: switch through the driver (init and pin are checked there)
    RelayStatus_t status = Relay_SetState((uint8_t)relay, on);
    if (status != RELAY_OK) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "hardware.relay_set: relay %d: %s", relay, Relay_StatusToStr(status));
        return NULL;
    }
    return val_new_void();
}

/**
 * @brief Function hardware.relay_toggle(int relay): switch a relay to the opposite state.
 * @param handle Calling runtime handle, used to raise errors.
 * @param argv Arguments: (relay), 1..MAX_RELAYS. Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Void value; NULL after raising an error.
 */
static value_t *hardware_relay_toggle(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    int relay = argv[0]->int_val;

    // Step 1: the uint8_t cast would wrap
    if (relay < 1 || relay > MAX_RELAYS) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "hardware.relay_toggle: relay must be 1..%d, got %d", MAX_RELAYS, relay);
        return NULL;
    }

    // Step 2: read the current state, then write the opposite (init and pin are checked by the driver)
    bool on = false;
    RelayStatus_t status = Relay_GetState((uint8_t)relay, &on);
    if (status == RELAY_OK) {
        status = Relay_SetState((uint8_t)relay, !on);
    }
    if (status != RELAY_OK) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "hardware.relay_toggle: relay %d: %s", relay, Relay_StatusToStr(status));
        return NULL;
    }
    return val_new_void();
}
#endif // USE_RELAY

/*---- PUBLIC FUNCTIONS ----*/

/**
 * @brief Make the module importable by scripts ('import hardware;').
 */
void hardware_module_init(void) {
    if (!ti_register_module(&s_hardware_module)) {
        LOG_ERROR("hardware module: registration failed");
    }
}
