#include "config.h"
#ifdef USE_RELAY
#include <Arduino.h>
#include "core/core_engine.h"
#include "services/web/web_template.h"
#include "services/logger/log_task.h"
#include "services/mqtt/mqtt_task.h"
#include "hardware/driver/xdrv_02_relay.h"

typedef struct {
    int8_t pin;
    bool active;
} RelayConfig_t;

static RelayConfig_t s_relays[MAX_RELAYS];
static bool s_initialized = false;   /* Set at the end of a successful SIG_INIT (at least one relay configured) */

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

RelayStatus_t Relay_GetState(uint8_t relay, bool *state) {
    // Step 1: driver ready, valid arguments
    if (!s_initialized) return RELAY_ERR_NOT_INIT;
    if (relay < 1 || relay > MAX_RELAYS) return RELAY_ERR_OUT_OF_RANGE;
    if (state == NULL) return RELAY_ERR_INVALID_ARG;

    // Step 2: the relay must have a pin
    const RelayConfig_t *r = &s_relays[relay - 1];
    if (!r->active) return RELAY_ERR_NO_PIN;

    // Step 3: read the output level
    *state = (digitalRead(r->pin) == HIGH);
    return RELAY_OK;
}

RelayStatus_t Relay_SetState(uint8_t relay, bool state) {
    // Step 1: driver ready, valid relay number
    if (!s_initialized) return RELAY_ERR_NOT_INIT;
    if (relay < 1 || relay > MAX_RELAYS) return RELAY_ERR_OUT_OF_RANGE;

    // Step 2: the relay must have a pin
    const RelayConfig_t *r = &s_relays[relay - 1];
    if (!r->active) return RELAY_ERR_NO_PIN;

    // Step 3: drive the output
    digitalWrite(r->pin, state ? HIGH : LOW);
    return RELAY_OK;
}

const char *Relay_StatusToStr(RelayStatus_t status) {
    switch (status) {
        case RELAY_OK:               return "OK";
        case RELAY_ERR_NOT_INIT:     return "Relay driver not initialized";
        case RELAY_ERR_OUT_OF_RANGE: return "Relay number out of range";
        case RELAY_ERR_NO_PIN:       return "Relay has no pin configured";
        case RELAY_ERR_INVALID_ARG:  return "Invalid argument";
        default:                     return "Unknown error";
    }
}

/**
 * @brief Control handler execution for relay commands.
 */
static void relay_cmd_handler(uint8_t index, const String &arg) {
    String cleanArg = arg;
    cleanArg.trim();
    cleanArg.toUpperCase();

    // Step 1: parse the argument
    bool on;
    if (cleanArg == "1" || cleanArg == "ON") {
        on = true;
    } else if (cleanArg == "0" || cleanArg == "OFF") {
        on = false;
    } else {
        LOG_WARNING_STR("Relay%d: invalid argument '%s'", index + 1, arg.c_str());
        return;
    }

    // Step 2: Relay_SetState checks range, init and pin
    RelayStatus_t status = Relay_SetState(index + 1, on);
    if (status != RELAY_OK) {
        LOG_ERROR_STR("Relay%d: %s", index + 1, Relay_StatusToStr(status));
        return;
    }
    LOG_INFO_STR("Relay%d set to %s", index + 1, on ? "ON" : "OFF");
}

static void relay1_cmd(const String &arg) { relay_cmd_handler(0, arg); }
static void relay2_cmd(const String &arg) { relay_cmd_handler(1, arg); }
static void relay3_cmd(const String &arg) { relay_cmd_handler(2, arg); }

/**
 * @brief Điểm cổng vào (Driver Dispatcher Entry Point) tiếp nhận và định tuyến các tín hiệu hệ thống cho Relay.
 * 
 * @details Hàm thực hiện cơ chế Signal Routing theo kiến trúc Event-Driven của Core Framework:
 *          - SIG_INIT: Khởi tạo phần cứng GPIO và đăng ký CLI commands.
 *          - SIG_WEB_POLL: Đọc trạng thái GPIO và đồng bộ hóa lên giao diện Web UI qua updateElementValue().
 *          - SIG_MQTT_PUBLISH: Thu thập trạng thái GPIO và đẩy vào gói tin telemetry định kỳ của MQTT Task.
 * 
 * @param[in] signal Tín hiệu sự kiện hệ thống được phát đi từ Core Engine hoặc các Task chuyên trách.
 * @return bool Trả về true nếu tín hiệu được xử lý thành công, false nếu tín hiệu bị bỏ qua hoặc không hỗ trợ.
 */
bool Xdrv2(Signal_t signal) {
    /*
     * CƠ CHẾ ĐỊNH TUYẾN TÍN HIỆU (Signal Routing Mechanism):
     * Căn cứ vào mã tín hiệu `signal`, hàm điều phối luồng thực thi tương ứng cho module phần cứng Relay:
     */
    switch (signal) {
        /*
         * TÍN HIỆU SIG_INIT: Khởi tạo driver khi hệ thống boot
         * 1. Đăng ký các câu lệnh điều khiển trực tiếp qua Serial / Web Terminal ("RELAY1", "RELAY2", "RELAY3").
         * 2. Quét bảng cấu hình phần cứng (is_use_name, get_gpio_by_name) để xác định chân GPIO cho từng relay.
         * 3. Cấu hình GPIO pinMode(OUTPUT), kéo mức logic LOW mặc định khi khởi động.
         * Script dùng relay qua module "hardware" (hardware.relay_get / relay_set).
         */
        case SIG_INIT: {
            // Step 1: CLI commands, always (Relay_SetState reports a relay without pin)
            register_cmd("RELAY1", relay1_cmd);
            register_cmd("RELAY2", relay2_cmd);
            register_cmd("RELAY3", relay3_cmd);

            // Step 2: resolve and configure the pin of every relay that has one
            for (uint8_t i = 0; i < MAX_RELAYS; i++) {
                String relay_name = "Relay" + String(i + 1);
                int8_t pin = is_use_name(relay_name.c_str()) ? get_gpio_by_name(relay_name.c_str()) : GPIO_INVALID;

                s_relays[i].pin    = pin;
                s_relays[i].active = (pin != GPIO_INVALID);
                if (s_relays[i].active) {
                    pinMode(pin, OUTPUT);
                    digitalWrite(pin, LOW);
                    LOG_INFO(relay_name + " on GPIO " + String(pin));
                }
            }

            // Step 3: last, so the pins above are configured before anyone may use them
            s_initialized = true;
            return true;
        }
        default:
            return false;
    }
}
#endif //__USE_RELAY
