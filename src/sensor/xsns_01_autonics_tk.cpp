#include "config.h"
#ifdef USE_AUTONICS_TK_SENSOR
#include <Arduino.h>
#include "core/core_engine.h"
#include "modbus.h"
#include "built_in.h"
#include "TienInterpreter.h"

#define START_REG_ADDR      0x03E8
#define REG_COUNT           6     
#define READ_INPUT_REG      0x04 

static uint8_t s_slave_id = 0x01;
static char pv_str[16] = "-1";
static char sv_str[16] = "-1";
static float s_last_pv = 0.0f;
static float s_last_sv = 0.0f;

/* -------------------------------------------------------------------------- */
/*                        INTERPRETER BUILT-IN FUNCTIONS                      */
/* -------------------------------------------------------------------------- */

/** @brief Built-in autonics_tk_set_slave_address function setting target Modbus slave address. */
static value_t *built_in_autonics_tk_set_slave_address(value_t **argv, int argc) {
    if (argc != 1 || argv == NULL || argv[0] == NULL || argv[0]->type != VAL_INT) {
        ti_log("[ERROR] %s: Expect 1 integer argument (slave_address 1-247)\n", BUILTIN_AUTONICS_TK_SET_SLAVE_ADDRESS);
        ti_fatal();
    }
    int addr = argv[0]->int_val;
    if (addr < 1 || addr > 247) {
        ti_log("[ERROR] %s: Invalid slave address %d (range 1-247)\n", BUILTIN_AUTONICS_TK_SET_SLAVE_ADDRESS, addr);
        ti_fatal();
    }
    s_slave_id = (uint8_t)addr;
    LOG_INFO("Autonics TK slave address set to " + String(s_slave_id));
    return val_new_bool(true);
}

/** @brief Built-in autonics_tk_get_pv function returning latest cached Process Value (PV). */
static value_t *built_in_autonics_tk_get_pv(value_t **argv, int argc) {
    (void)argv;
    (void)argc;
    return val_new_float(s_last_pv);
}

/** @brief Built-in autonics_tk_get_sv function returning latest cached Set Value (SV). */
static value_t *built_in_autonics_tk_get_sv(value_t **argv, int argc) {
    (void)argv;
    (void)argc;
    return val_new_float(s_last_sv);
}

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
        LOG_WARNING("One or more RS485 pins are not configured.");
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

static void ProcessModbusPoll() {
    SendRetType ret = ModbusSend(s_slave_id, READ_INPUT_REG, START_REG_ADDR, REG_COUNT, 200);
    if (ret == ESEND_NOERR) {
        UCHAR rx_buf[256];
        USHORT rx_len = 0;
        ModbusReceive(rx_buf, &rx_len);

        if (rx_len < (REG_COUNT * 2)) {
            LOG_WARNING("[Xdrv2] Modbus response frame incomplete!");
            return;
        }

        AutonicsTKData data;
        ParseTKRegisters(rx_buf, rx_len, &data);
        FormatValueWithDecimal(data.raw_pv, data.decimal_point, pv_str, sizeof(pv_str));
        FormatValueWithDecimal(data.raw_sv, data.decimal_point, sv_str, sizeof(sv_str));
        s_last_pv = (float)atof(pv_str);
        s_last_sv = (float)atof(sv_str);
        const char* unit_str = GetUnitString(data.unit_code);

        // Push Telemetry directly to web REST API
        updateElementValue("Autonics PV", String(pv_str) + " " + String(unit_str));
        updateElementValue("Autonics SV", String(sv_str) + " " + String(unit_str));
    } 
    else {
        LOG_WARNING("[Xdrv2] Failed to parse Modbus registers.");
        snprintf(pv_str, sizeof(pv_str), "-1");
        snprintf(sv_str, sizeof(sv_str), "-1");
        s_last_pv = -1.0f;
        s_last_sv = -1.0f;
    }
}

bool Xsns1(Signal_t signal) {
    switch (signal) {
        case SIG_INIT: {
            register_builtin_function(BUILTIN_AUTONICS_TK_SET_SLAVE_ADDRESS, built_in_autonics_tk_set_slave_address);
            register_builtin_function(BUILTIN_AUTONICS_TK_GET_PV, built_in_autonics_tk_get_pv);
            register_builtin_function(BUILTIN_AUTONICS_TK_GET_SV, built_in_autonics_tk_get_sv);

            if (!CheckRS485PinConfig()) {
                return false;
            }

            bool ok = ModbusInit(9600, RTU_8N1, rs485_tx_pin, rs485_rx_pin, rs485_de_pin);
            if (!ok) {
                LOG_ERROR("[Xdrv2] Init Modbus failed!");
                return false;
            }

            ModbusStart();
            modbus_initialized = true;
            LOG_INFO("[Xdrv2] Init Modbus successfully");
            return true;
        }

        case SIG_1SEC: {
            ProcessModbusPoll();
            return true;
        }

        case SIG_WEB_POLL: {
            FormatValueWithDecimal(atoi(pv_str), 0, pv_str, sizeof(pv_str));
            FormatValueWithDecimal(atoi(sv_str), 0, sv_str, sizeof(sv_str));
            updateElementValue("Autonics PV", String(pv_str));
            updateElementValue("Autonics SV", String(sv_str));
            return true;
        }
        
        case SIG_MQTT_PUBLISH: {
            FormatValueWithDecimal(atoi(pv_str), 0, pv_str, sizeof(pv_str));
            FormatValueWithDecimal(atoi(sv_str), 0, sv_str, sizeof(sv_str));
            mqtt_add_telemetry("Autonics PV", pv_str);
            mqtt_add_telemetry("Autonics SV", sv_str);
            return true;
        }
        default:
            return false;
    }
}

#endif // USE_AUTONICS_TK_SENSOR