#include "config.h"
#include "core/log_task.h"
#include "core/web_server_task.h"
#ifdef USE_AUTONICS_TK_SENSOR
#include <Arduino.h>
#include "core/core_engine.h"
#include "modbus.h"
#include "cmd.h"

/* Hard code section */
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#define SLAVE_ID_SOLE           0x02

// Default threshold values defined at top of file
#define DEFAULT_SOLE_TEMP_MIN   50.0f
#define DEFAULT_SOLE_TEMP_MAX   60.0f
#define DEFAULT_UPPER_TEMP_MIN  55.0f
#define DEFAULT_UPPER_TEMP_MAX  65.0f

// Endpoint URL to fetch temperature thresholds JSON
static const char* HARDCODE_HTTP_URL = "http://192.168.1.13:82/api/chambertemp/std?ip=192.168.0.215";
static const char* HARDCODE_FILE_PATH = "/xsns_autonics_tk.txt";

static float g_sole_temp_min  = DEFAULT_SOLE_TEMP_MIN;
static float g_sole_temp_max  = DEFAULT_SOLE_TEMP_MAX;
static float g_upper_temp_min = DEFAULT_UPPER_TEMP_MIN;
static float g_upper_temp_max = DEFAULT_UPPER_TEMP_MAX;

static char sole_pv_str[16] = "-1";
static char sole_sv_str[16] = "-1";

static bool parseAndSaveThresholdJson(const String& jsonStr, bool saveToFile) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, jsonStr);
    if (!err) {
        if (doc.containsKey("SOLE_TEMP_MIN"))  g_sole_temp_min  = doc["SOLE_TEMP_MIN"].as<float>();
        if (doc.containsKey("SOLE_TEMP_MAX"))  g_sole_temp_max  = doc["SOLE_TEMP_MAX"].as<float>();
        if (doc.containsKey("UPPER_TEMP_MIN")) g_upper_temp_min = doc["UPPER_TEMP_MIN"].as<float>();
        if (doc.containsKey("UPPER_TEMP_MAX")) g_upper_temp_max = doc["UPPER_TEMP_MAX"].as<float>();

        if (saveToFile) {
            File f = LittleFS.open(HARDCODE_FILE_PATH, "w");
            if (f) {
                f.print(jsonStr);
                f.close();
                LOG_INFO("[HardCode] Saved threshold config to file " + String(HARDCODE_FILE_PATH));
            } else {
                LOG_WARNING("[HardCode] Failed to open file for writing: " + String(HARDCODE_FILE_PATH));
            }
        }
        return true;
    }
    return false;
}

static void saveCurrentThresholdsToFlash() {
    JsonDocument doc;
    doc["SOLE_TEMP_MIN"]  = g_sole_temp_min;
    doc["SOLE_TEMP_MAX"]  = g_sole_temp_max;
    doc["UPPER_TEMP_MIN"] = g_upper_temp_min;
    doc["UPPER_TEMP_MAX"] = g_upper_temp_max;

    File f = LittleFS.open(HARDCODE_FILE_PATH, "w");
    if (f) {
        serializeJson(doc, f);
        f.close();
        LOG_INFO("[HardCode] Saved threshold config to file " + String(HARDCODE_FILE_PATH));
    } else {
        LOG_WARNING("[HardCode] Failed to open file for writing: " + String(HARDCODE_FILE_PATH));
    }
}

static void InitHardcodeThresholds() {
    bool loaded = false;

    // Step 1: Load from flash file /xsns_autonics_tk.txt
    if (LittleFS.exists(HARDCODE_FILE_PATH)) {
        File f = LittleFS.open(HARDCODE_FILE_PATH, "r");
        if (f) {
            String content = f.readString();
            f.close();
            if (parseAndSaveThresholdJson(content, false)) {
                loaded = true;
                LOG_INFO("[HardCode] Loaded thresholds from file " + String(HARDCODE_FILE_PATH) + 
                         " -> Upper Min: " + String(g_upper_temp_min) + 
                         ", Upper Max: " + String(g_upper_temp_max) + 
                         ", Sole Min: " + String(g_sole_temp_min) + 
                         ", Sole Max: " + String(g_sole_temp_max));
            }
        }
    }

    // Step 2: Fall back to top defined default thresholds if file loading failed, save to memory & flash
    if (!loaded) {
        g_sole_temp_min  = DEFAULT_SOLE_TEMP_MIN;
        g_sole_temp_max  = DEFAULT_SOLE_TEMP_MAX;
        g_upper_temp_min = DEFAULT_UPPER_TEMP_MIN;
        g_upper_temp_max = DEFAULT_UPPER_TEMP_MAX;

        saveCurrentThresholdsToFlash();
        LOG_INFO("[HardCode] Used fallback default defined thresholds -> Upper Min: " + String(g_upper_temp_min) + 
                 ", Upper Max: " + String(g_upper_temp_max) + 
                 ", Sole Min: " + String(g_sole_temp_min) + 
                 ", Sole Max: " + String(g_sole_temp_max));
    }
}

static void FetchHardcodeThresholdsHttp() {
    if (!is_wifi_connected()) return;

    HTTPClient http;
    http.begin(HARDCODE_HTTP_URL);
    http.setTimeout(3000);

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        if (parseAndSaveThresholdJson(payload, true)) {
            LOG_INFO("[HardCode] Loaded thresholds via HTTP GET on Wi-Fi Connect -> Upper Min: " + String(g_upper_temp_min) + 
                     ", Upper Max: " + String(g_upper_temp_max) + 
                     ", Sole Min: " + String(g_sole_temp_min) + 
                     ", Sole Max: " + String(g_sole_temp_max));
        }
    } else {
        LOG_WARNING("[HardCode] HTTP GET failed with code: " + String(httpCode));
    }
    http.end();
}

static void ProcessHardcodeThresholdCheck(float upper_pv, float sole_pv) {
    bool upper_valid = (upper_pv >= 0.0f);
    bool sole_valid  = (sole_pv >= 0.0f);

    // Both sensors disconnected: Log warning and abort execution
    if (!upper_valid && !sole_valid) {
        LOG_WARNING("[HardCode] Both sensors disconnected! Aborting threshold check.");
        return;
    }

    // Evaluate heat demand for valid sensors
    bool upper_needs_on = upper_valid && (upper_pv < g_upper_temp_min || upper_pv > g_upper_temp_max);
    bool sole_needs_on  = sole_valid  && (sole_pv < g_sole_temp_min   || sole_pv > g_sole_temp_max);

    // Turn ON relay if at least one connected sensor triggers threshold
    if (upper_needs_on || sole_needs_on) {
        LOG_WARNING("[HardCode] Threshold triggered (Upper ON req: " + String(upper_needs_on) + 
                    ", Sole ON req: " + String(sole_needs_on) + "). Posting: " RELAY1_CMD ": ON");
        postIncomingCommand(RELAY1_CMD ": ON");
    } 
    // Turn OFF relay when all connected sensors are in normal condition
    else {
        if(upper_valid && sole_valid) {
          LOG_INFO("[HardCode] Normal condition (or non-trigger state). Posting: " RELAY1_CMD ": OFF");
          postIncomingCommand(RELAY1_CMD ": OFF");
        }
    }
}
/* End Hard code section */

#define SLAVE_ID_UPPER      0x01
#define START_REG_ADDR      0x03E8
#define REG_COUNT           6     
#define READ_INPUT_REG      0x04 

static char upper_pv_str[16] = "-1";
static char upper_sv_str[16] = "-1";

struct AutonicsTKData {
    uint16_t raw_pv;
    uint16_t raw_sv;
    uint8_t decimal_point;
    uint8_t unit_code;
};

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
    float upper_pv_val = -1.0f;
    float sole_pv_val  = -1.0f;

    // --- 1. Poll Slave 0x01 (Upper Controller) ---
    SendRetType ret1 = ModbusSend(SLAVE_ID_UPPER, READ_INPUT_REG, START_REG_ADDR, REG_COUNT, 200);
    if (ret1 == ESEND_NOERR) {
        UCHAR rx_buf[256];
        USHORT rx_len = 0;
        ModbusReceive(rx_buf, &rx_len);
        if (rx_len >= (REG_COUNT * 2)) {
            AutonicsTKData data;
            ParseTKRegisters(rx_buf, rx_len, &data);
            FormatValueWithDecimal(data.raw_pv, data.decimal_point, upper_pv_str, sizeof(upper_pv_str));
            FormatValueWithDecimal(data.raw_sv, data.decimal_point, upper_sv_str, sizeof(upper_sv_str));
            upper_pv_val = atof(upper_pv_str);
        } else {
            snprintf(upper_pv_str, sizeof(upper_pv_str), "-1");
            snprintf(upper_sv_str, sizeof(upper_sv_str), "-1");
        }
    } else {
        snprintf(upper_pv_str, sizeof(upper_pv_str), "-1");
        snprintf(upper_sv_str, sizeof(upper_sv_str), "-1");
    }

    /* Hard code section: Poll Slave 0x02 (Sole Controller) & Threshold Check */
    vTaskDelay(pdMS_TO_TICKS(50));

    SendRetType ret2 = ModbusSend(SLAVE_ID_SOLE, READ_INPUT_REG, START_REG_ADDR, REG_COUNT, 200);
    if (ret2 == ESEND_NOERR) {
        UCHAR rx_buf[256];
        USHORT rx_len = 0;
        ModbusReceive(rx_buf, &rx_len);
        if (rx_len >= (REG_COUNT * 2)) {
            AutonicsTKData data;
            ParseTKRegisters(rx_buf, rx_len, &data);
            FormatValueWithDecimal(data.raw_pv, data.decimal_point, sole_pv_str, sizeof(sole_pv_str));
            FormatValueWithDecimal(data.raw_sv, data.decimal_point, sole_sv_str, sizeof(sole_sv_str));
            sole_pv_val = atof(sole_pv_str);
        } else {
            snprintf(sole_pv_str, sizeof(sole_pv_str), "-1");
            snprintf(sole_sv_str, sizeof(sole_sv_str), "-1");
        }
    } else {
        snprintf(sole_pv_str, sizeof(sole_pv_str), "-1");
        snprintf(sole_sv_str, sizeof(sole_sv_str), "-1");
    }
    ProcessHardcodeThresholdCheck(upper_pv_val, sole_pv_val);
    /* End Hard code section */
}

bool Xsns1(Signal_t signal) {
    switch (signal) {
        case SIG_INIT: {
            /* Hard code section */
            InitHardcodeThresholds();
            /* End Hard code section */

            if (!CheckRS485PinConfig()) {
                return false;
            }

            if (!isModbusInit()) {
                bool ok = ModbusInit(9600, RTU_8N1, rs485_tx_pin, rs485_rx_pin, rs485_de_pin);
                if (!ok) {
                    LOG_ERROR("[Xdrv2] Init Modbus failed!");
                    return false;
                }
                ModbusStart();
                LOG_INFO("[Xdrv2] Init Modbus successfully");
            }
            return true;
        }

        case SIG_WIFI_CONNECTED: {
            /* Hard code section: Fetch thresholds via HTTP GET upon Wi-Fi connect */
            FetchHardcodeThresholdsHttp();
            return true;
        }

        case SIG_1SEC: {
            ProcessModbusPoll();
            rule_on_event("TK4S_UPPER", atof(upper_pv_str));
            /* Hard code section */
            rule_on_event("TK4S_SOLE",  atof(sole_pv_str));
            /* End Hard code section */

            return true;
        }

        case SIG_WEB_POLL: {
            updateElementValue("Upper PV", String(upper_pv_str));
            updateElementValue("Upper SV", String(upper_sv_str));
            /* Hard code section */
            updateElementValue("Sole PV",  String(sole_pv_str));
            updateElementValue("Sole SV",  String(sole_sv_str));
            char sole_temp_min_str[10]; 
            char sole_temp_max_str[10]; 
            char upper_temp_min_str[10]; 
            char upper_temp_max_str[10]; 
            snprintf(sole_temp_min_str,  sizeof(sole_temp_min_str),  "%.2f", g_sole_temp_min);
            snprintf(sole_temp_max_str,  sizeof(sole_temp_max_str),  "%.2f", g_sole_temp_max);
            snprintf(upper_temp_min_str, sizeof(upper_temp_min_str), "%.2f", g_upper_temp_min);
            snprintf(upper_temp_max_str, sizeof(upper_temp_max_str), "%.2f", g_upper_temp_max);
            updateElementValue("SOLE_TEMP_MIN", String(sole_temp_min_str));
            updateElementValue("SOLE_TEMP_MAX", String(sole_temp_max_str));
            updateElementValue("UPPER_TEMP_MIN", String(upper_temp_min_str));
            updateElementValue("UPPER_TEMP_MAX", String(upper_temp_max_str)); 
            /* End Hard code section */
            return true;
        }
        
        case SIG_MQTT_PUBLISH: {
            mqtt_add_telemetry("Upper PV", upper_pv_str);
            mqtt_add_telemetry("Upper SV", upper_sv_str);
            /* Hard code section */
            mqtt_add_telemetry("Sole PV",  sole_pv_str);
            mqtt_add_telemetry("Sole SV",  sole_sv_str);
            /* End Hard code section */
            return true;
        }
        default:
            return false;
    }
}

#endif // USE_AUTONICS_TK_SENSOR
