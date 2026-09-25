#include "config.h"
#ifdef USE_AUTONICS_TK_ADAPTER
#include <Arduino.h>
#include "core/core_engine.h"
#include "modbus.h"
#include "built_in.h"
#include "TienInterpreter.h"

#define START_REG_ADDR      0x03E8
#define REG_COUNT           6     
#define READ_INPUT_REG      0x04 

/* -------------------------------------------------------------------------- */
/*                        INTERPRETER BUILT-IN FUNCTIONS                      */
/* -------------------------------------------------------------------------- */

struct AutonicsTKData {
    uint16_t raw_pv;
    uint16_t raw_sv;
    uint8_t decimal_point;
    uint8_t unit_code;
};

static bool modbus_initialized = false;
static int8_t rs485_tx_pin = GPIO_INVALID;
static int8_t rs485_rx_pin = GPIO_INVALID;
static int8_t rs485_de_pin = GPIO_INVALID;
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

static void ParseTKRegisters(const uint8_t* rx_buf, uint16_t rx_len, AutonicsTKData* out_data) {
    out_data->raw_pv        = (rx_buf[0] << 8) | rx_buf[1];
    out_data->decimal_point = (rx_buf[2] << 8) | rx_buf[3];
    out_data->unit_code     = (rx_buf[4] << 8) | rx_buf[5];
    out_data->raw_sv        = (rx_buf[6] << 8) | rx_buf[7];
}

static bool CheckRS485PinConfig() {
    if (!is_use_name("RS485_TX") || !is_use_name("RS485_RX")) {
        return false;
    }

    rs485_tx_pin = get_gpio_by_name("RS485_TX");
    rs485_rx_pin = get_gpio_by_name("RS485_RX");
    if (is_use_name("RS485_DE"))
        rs485_de_pin = get_gpio_by_name("RS485_DE");

    if (rs485_tx_pin == GPIO_INVALID || rs485_rx_pin == GPIO_INVALID) {
        LOG_ERROR("Failed to resolve GPIO pins for RS485.");
        return false;
    }

    LOG_INFO("RS485 pins resolved | TX: " + String(rs485_tx_pin) + " | RX: " + String(rs485_rx_pin) + " | DE: " + String(rs485_de_pin));
    return true;
}

static value_t *autonic_read(value_t **argv, int argc) {
    int slave_id = argv[0]->int_val;
    value_t *dict_val = val_new_dict();

    SendRetType ret = ModbusSend(slave_id, READ_INPUT_REG, START_REG_ADDR, REG_COUNT, 200);
    if (ret == ESEND_NOERR) {
        UCHAR rx_buf[256];
        USHORT rx_len = 0;
        ModbusReceive(rx_buf, &rx_len);

        /* Each register = 2 bytes, not sure the lib already capture this error or not*/
        if (rx_len < (REG_COUNT * 2)) {
            LOG_WARNING("[Xadp1] Modbus response frame incomplete!");
            val_dict_set(dict_val->dict_val, "success", val_new_bool(false));
            val_dict_set(dict_val->dict_val, "error", val_new_string("Invalid response"));
            return dict_val;
        }

        /* Format data from buffer to value */
        AutonicsTKData data;
        ParseTKRegisters(rx_buf, rx_len, &data);

        /* Get the value in string format */
        char pv_str[20], sv_str[20];
        const char* unit_str = GetUnitString(data.unit_code);
        FormatValueWithDecimal(data.raw_pv, data.decimal_point, pv_str, sizeof(pv_str));
        FormatValueWithDecimal(data.raw_sv, data.decimal_point, sv_str, sizeof(sv_str));

        val_dict_set(dict_val->dict_val, "success", val_new_bool(true));
        val_dict_set(dict_val->dict_val, "pv", val_new_float(atof(pv_str)));
        val_dict_set(dict_val->dict_val, "sv", val_new_float(atof(sv_str)));
        val_dict_set(dict_val->dict_val, "unit", val_new_string(unit_str));
    }
    else {
        val_dict_set(dict_val->dict_val, "success", val_new_bool(false));
        val_dict_set(dict_val->dict_val, "error", val_new_string(ModbusErrToStr(ret)));
        LOG_WARNING("[Xadp1] Failed to read Modbus.");
    }
    return dict_val;
}

bool Xadp1(Signal_t signal) {
    switch (signal) {
        case SIG_INIT: {
            if (!CheckRS485PinConfig()) {
                return false;
            }
            bool ok = ModbusInit(9600, RTU_8N1, rs485_tx_pin, rs485_rx_pin, rs485_de_pin);
            if (!ok) {
                LOG_ERROR("[Xadp1] Init Modbus failed!");
                return false;
            }
            ModbusStart();
            modbus_initialized = true;
            /* Register builtin */
            static param_t tk_addr_param[] = { { VAL_INT, (char*)"slave_address" } };
            register_builtin_function("autonics_read", VAL_DICT, tk_addr_param, 1, autonic_read);
            LOG_INFO("[Xadp1] Init Modbus successfully");
            return true;
        }
        default:
            return true;
    }
}

#endif // USE_AUTONICS_TK_ADAPTER
