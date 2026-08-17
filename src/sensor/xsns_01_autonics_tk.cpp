#include "config.h"
#ifdef USE_AUTONICS_TK_SENSOR
#include <Arduino.h>
#include "core/core_engine.h"
#include "modbus.h"

#define SLAVE_ID            0x01
#define START_REG_ADDR      0x03E8
#define REG_COUNT           6     
#define READ_INPUT_REG      0x04 

static char pv_str[16];
static char sv_str[16];

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
    SendRetType ret = ModbusSend(SLAVE_ID, READ_INPUT_REG, START_REG_ADDR, REG_COUNT, 200);
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
        const char* unit_str = GetUnitString(data.unit_code);

        // Push Telemetry directly to web REST API
        updateElementValue("Autonics PV", String(pv_str) + " " + String(unit_str));
        updateElementValue("Autonics SV", String(sv_str) + " " + String(unit_str));
    } 
    else {
        LOG_WARNING("[Xdrv2] Failed to parse Modbus registers.");
        snprintf(pv_str, sizeof(pv_str), "-1");
        snprintf(sv_str, sizeof(sv_str), "-1");
    }
}

bool Xsns1(Signal_t signal) {
    switch (signal) {
        case SIG_INIT: {
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