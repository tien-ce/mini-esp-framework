#include "config.h"
#ifdef USE_MODBUS_MASTER
#ifdef USE_AUTONICS_TK_ADAPTER
#include <Arduino.h>
#include "core/core_engine.h"
#include "core/core_log.h"
#include "hardware/protocol/modbus_master.h"
#include "built_in.h"
#include "TienInterpreter.h"

#define START_REG_ADDR      0x03E8
#define REG_COUNT           6     
#define READ_INPUT_REG      0x04
#define READ_TIMEOUT_MS     500

extern ModbusMasterStatus_t modbus_send_async(ti_handle_t handle,
                                              uint8_t port, uint8_t slave, uint8_t function,
                                              uint16_t address, uint16_t quantity, uint32_t timeout_ms,
                                              uint8_t *buf, size_t *len);

/* -------------------------------------------------------------------------- */
/*                        INTERPRETER BUILT-IN FUNCTIONS                      */
/* -------------------------------------------------------------------------- */

struct AutonicsTKData {
    uint16_t raw_pv;
    uint16_t raw_sv;
    uint8_t decimal_point;
    uint8_t unit_code;
};

static const char* GetUnitString(uint8_t unit_code) {
    switch (unit_code) {
        case 0: return "°C";
        case 1: return "°F";
        case 2: return "00";
        default: return "";
    }
}

static void FormatValueWithDecimal(uint16_t raw_val, uint8_t dp, char* out_buf, size_t buf_size) {
    switch (dp) {
        case 1:
            snprintf(out_buf, buf_size, "%u.%u", raw_val / 10, raw_val % 10);
            break;
        case 2:
            snprintf(out_buf, buf_size, "%u.%02u", raw_val / 100, raw_val % 100);
            break;
        case 3:
            snprintf(out_buf, buf_size, "%u.%03u", raw_val / 1000, raw_val % 1000);
            break;
        case 0:
        default:
            snprintf(out_buf, buf_size, "%u", raw_val);
            break;
    }
}

static void ParseTKRegisters(const uint16_t* regs, AutonicsTKData* out_data) {
    out_data->raw_pv        = regs[0];
    out_data->decimal_point = regs[1];
    out_data->unit_code     = regs[2];
    out_data->raw_sv        = regs[3];
}

static value_t *autonic_read(ti_handle_t handle, value_t **argv, int argc) {
    int port     = argv[0]->int_val;
    int slave_id = argv[1]->int_val;

    /* Script mistakes are raised (also stops a wrong number wrapping in the casts below) */
    if (port < 1 || port > SERIAL_BUS_PORT_COUNT) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "autonics_read: port must be 1..%d, got %d",
                       SERIAL_BUS_PORT_COUNT, port);
        return NULL;
    }
    if (slave_id < 0 || slave_id > 255) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "autonics_read: slave_address must be 0..255, got %d", slave_id);
        return NULL;
    }

    value_t *dict_val = val_new_dict();

    uint8_t answer[MODBUS_MASTER_MAX_RESPONSE];
    size_t answer_len = sizeof(answer);
    ModbusMasterStatus_t ret = modbus_send_async(handle, (uint8_t)port, (uint8_t)slave_id, READ_INPUT_REG, START_REG_ADDR,
                                                 REG_COUNT, READ_TIMEOUT_MS, answer, &answer_len);
    val_dict_set(dict_val->dict_val, "status", val_new_string(ModbusMaster_StatusToStr(ret)));
    if (ret == MBM_OK) {
        /* Registers arrive as bytes, high byte first */
        uint16_t regs[REG_COUNT];
        for (int i = 0; i < REG_COUNT; i++) {
            regs[i] = (uint16_t)((answer[2 * i] << 8) | answer[2 * i + 1]);
        }

        /* Format data from registers to value */
        AutonicsTKData data;
        ParseTKRegisters(regs, &data);

        /* Get the value in string format */
        char pv_str[20], sv_str[20];
        const char* unit_str = GetUnitString(data.unit_code);
        FormatValueWithDecimal(data.raw_pv, data.decimal_point, pv_str, sizeof(pv_str));
        FormatValueWithDecimal(data.raw_sv, data.decimal_point, sv_str, sizeof(sv_str));

        val_dict_set(dict_val->dict_val, "pv", val_new_float(atof(pv_str)));
        val_dict_set(dict_val->dict_val, "sv", val_new_float(atof(sv_str)));
        val_dict_set(dict_val->dict_val, "unit", val_new_string(unit_str));
    }
    else {
        LOG_WARNING(String("[Xadp1] Failed to read Modbus: ") + ModbusMaster_StatusToStr(ret));
    }
    return dict_val;
}

bool Xadp1(Signal_t signal) {
    switch (signal) {
        case SIG_INIT: {
            /* The port is opened by the script (modbus_open), the adapter only registers its built-in */
            static param_t tk_params[] = { { VAL_INT, (char*)"port" }, { VAL_INT, (char*)"slave_address" } };
            register_builtin_function("autonics_read", VAL_DICT, tk_params, 2, autonic_read);
            LOG_INFO("[Xadp1] Init successfully");
            return true;
        }
        default:
            return true;
    }
}
#endif // USE_AUTONICS_TK_ADAPTER
#endif // MODBUS MASTER
