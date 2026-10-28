#include "config.h"
#ifdef USE_MODBUS_MASTER
#ifdef USE_OMRON_E5CC_ADAPTER
#include <Arduino.h>
#include "core/core_engine.h"
#include "core/core_log.h"
#include "hardware/protocol/modbus_master.h"
#include "built_in.h"
#include "TienInterpreter.h"

/* Register map: docs/hardware/omron_e5cc_modbus_address_table.md (2-byte mode) */
#define READ_HOLDING_REG    0x03
#define REG_DECIMAL_POINT   0x2410      /* Decimal Point Monitor, 1 register */
#define REG_INPUT_TYPE      0x2C00      /* Input Type, followed by Temperature Unit (0x2C01) */
#define REG_OPERATION       0x2000      /* PV, Status, Internal SP, Heater Current, MV (Heating): 5 registers */
#define REG_OPERATION_COUNT 5
#define REG_SET_POINT       0x2103      /* Set Point, 1 register */
#define STATUS_BIT_INPUT_ERROR  6       /* Status (lower word): input error, sensor open / short */
#define STATUS_BIT_HEATING_OUT  8       /* Status (lower word): control output (heating) ON */
#define READ_TIMEOUT_MS     500

/**
 * @brief Read consecutive holding registers of one slave.
 * @param port Modbus port number.
 * @param slave Slave address.
 * @param address First register.
 * @param count Number of registers, at most MODBUS_MASTER_MAX_RESPONSE / 2.
 * @param regs Receives the registers (count entries).
 * @return MBM_OK or the error of ModbusMaster_Send.
 */
static ModbusMasterStatus_t ReadRegisters(uint8_t port, uint8_t slave, uint16_t address, uint16_t count, uint16_t *regs) {
    uint8_t answer[MODBUS_MASTER_MAX_RESPONSE];
    size_t answer_len = sizeof(answer);
    ModbusMasterStatus_t ret = ModbusMaster_Send(port, slave, READ_HOLDING_REG, address, count,
                                                 READ_TIMEOUT_MS, answer, &answer_len);
    if (ret != MBM_OK) return ret;

    /* Registers arrive as bytes, high byte first */
    for (uint16_t i = 0; i < count; i++) {
        regs[i] = (uint16_t)((answer[2 * i] << 8) | answer[2 * i + 1]);
    }
    return MBM_OK;
}

/**
 * @brief Built-in omron_e5cc_read_config(int port, int slave_address): the static settings, read once by the script.
 * @param handle Calling runtime handle, used to report argument errors.
 * @param argv Arguments: (port, slave_address). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Dict {"status", "decimal_point", "unit", "input_type"} (only "status" on error); NULL after raising an error.
 */
static value_t *omron_e5cc_read_config(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    int port  = argv[0]->int_val;
    int slave = argv[1]->int_val;

    // Step 1: script mistakes are raised (also stops a wrong number wrapping in the casts below)
    if (port < 1 || port > SERIAL_BUS_PORT_COUNT) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "omron_e5cc_read_config: port must be 1..%d, got %d",
                       SERIAL_BUS_PORT_COUNT, port);
        return NULL;
    }
    if (slave < 0 || slave > 255) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "omron_e5cc_read_config: slave_address must be 0..255, got %d", slave);
        return NULL;
    }

    value_t *result = val_new_dict();
    if (result == NULL) {
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "omron_e5cc_read_config: cannot allocate the result");
        return NULL;
    }

    // Step 2: transaction 1 = decimal point, transaction 2 = input type + temperature unit
    uint16_t decimal_point = 0;
    uint16_t type_unit[2] = { 0, 0 };
    ModbusMasterStatus_t ret = ReadRegisters((uint8_t)port, (uint8_t)slave, REG_DECIMAL_POINT, 1, &decimal_point);
    if (ret == MBM_OK) {
        ret = ReadRegisters((uint8_t)port, (uint8_t)slave, REG_INPUT_TYPE, 2, type_unit);
    }

    // Step 3: build the result; val_dict_set takes the value even when it fails, so every call is made
    ti_status_t st = val_dict_set(result->dict_val, "status", val_new_string(ModbusMaster_StatusToStr(ret)));
    if (ret == MBM_OK) {
        ti_status_t s1 = val_dict_set(result->dict_val, "decimal_point", val_new_int(decimal_point));
        ti_status_t s2 = val_dict_set(result->dict_val, "unit", val_new_string(type_unit[1] == 0 ? "C" : "F"));
        ti_status_t s3 = val_dict_set(result->dict_val, "input_type", val_new_int(type_unit[0]));
        if (st == TI_OK) st = s1;
        if (st == TI_OK) st = s2;
        if (st == TI_OK) st = s3;
    }
    else {
        LOG_WARNING(String("[Xadp2] Failed to read config: ") + ModbusMaster_StatusToStr(ret));
    }
    if (st != TI_OK) {
        val_free(result);
        ti_raise_error(handle, st, "omron_e5cc_read_config: cannot build the result");
        return NULL;
    }
    return result;
}

/**
 * @brief Built-in omron_e5cc_read_value(int port, int slave_address): the cyclic values, raw (not scaled).
 * @details pv and sv are scaled by 10^decimal_point, heater_current and mv by 10; the script divides.
 * @param handle Calling runtime handle, used to report argument errors.
 * @param argv Arguments: (port, slave_address). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Dict {"status", "pv", "sv", "mv", "heater_current", "sensor_error", "heating"} (only "status" on error);
 *         NULL after raising an error.
 */
static value_t *omron_e5cc_read_value(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    int port  = argv[0]->int_val;
    int slave = argv[1]->int_val;

    // Step 1: script mistakes are raised (also stops a wrong number wrapping in the casts below)
    if (port < 1 || port > SERIAL_BUS_PORT_COUNT) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "omron_e5cc_read_value: port must be 1..%d, got %d",
                       SERIAL_BUS_PORT_COUNT, port);
        return NULL;
    }
    if (slave < 0 || slave > 255) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "omron_e5cc_read_value: slave_address must be 0..255, got %d", slave);
        return NULL;
    }

    value_t *result = val_new_dict();
    if (result == NULL) {
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "omron_e5cc_read_value: cannot allocate the result");
        return NULL;
    }

    // Step 2: transaction 1 = PV / status / heater current / MV, transaction 2 = set point
    uint16_t op[REG_OPERATION_COUNT] = { 0 };
    uint16_t sp = 0;
    ModbusMasterStatus_t ret = ReadRegisters((uint8_t)port, (uint8_t)slave, REG_OPERATION, REG_OPERATION_COUNT, op);
    if (ret == MBM_OK) {
        ret = ReadRegisters((uint8_t)port, (uint8_t)slave, REG_SET_POINT, 1, &sp);
    }

    // Step 3: build the result; val_dict_set takes the value even when it fails, so every call is made
    ti_status_t st = val_dict_set(result->dict_val, "status", val_new_string(ModbusMaster_StatusToStr(ret)));
    if (ret == MBM_OK) {
        ti_status_t s[6];
        s[0] = val_dict_set(result->dict_val, "pv", val_new_int((int16_t)op[0]));
        s[1] = val_dict_set(result->dict_val, "sv", val_new_int((int16_t)sp));
        s[2] = val_dict_set(result->dict_val, "mv", val_new_int((int16_t)op[4]));
        s[3] = val_dict_set(result->dict_val, "heater_current", val_new_int(op[3]));
        s[4] = val_dict_set(result->dict_val, "sensor_error", val_new_bool((op[1] >> STATUS_BIT_INPUT_ERROR) & 1));
        s[5] = val_dict_set(result->dict_val, "heating", val_new_bool((op[1] >> STATUS_BIT_HEATING_OUT) & 1));
        for (int i = 0; i < 6 && st == TI_OK; i++) st = s[i];
    }
    else {
        LOG_WARNING(String("[Xadp2] Failed to read value: ") + ModbusMaster_StatusToStr(ret));
    }
    if (st != TI_OK) {
        val_free(result);
        ti_raise_error(handle, st, "omron_e5cc_read_value: cannot build the result");
        return NULL;
    }
    return result;
}

bool Xadp2(Signal_t signal) {
    switch (signal) {
        case SIG_INIT: {
            /* The port is opened by the script (modbus_open), the adapter only registers its built-ins */
            static param_t e5cc_params[] = { { VAL_INT, (char*)"port" }, { VAL_INT, (char*)"slave_address" } };
            register_builtin_function("omron_e5cc_read_config", VAL_DICT, e5cc_params, 2, omron_e5cc_read_config);
            register_builtin_function("omron_e5cc_read_value", VAL_DICT, e5cc_params, 2, omron_e5cc_read_value);
            LOG_INFO("[Xadp2] Init successfully");
            return true;
        }
        default:
            return true;
    }
}
#endif // USE_OMRON_E5CC_ADAPTER
#endif // MODBUS MASTER
