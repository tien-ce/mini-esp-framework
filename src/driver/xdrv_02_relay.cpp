#include "config.h"
#ifdef USE_RELAY
#include <Arduino.h>
#include "core/core_engine.h"

#define MAX_RELAYS 3

typedef struct {
    int8_t pin;
    bool active;
    String name;
} RelayConfig_t;

static RelayConfig_t s_relays[MAX_RELAYS];

/**
 * @brief Control handler execution for relay commands.
 */
static void relay_cmd_handler(uint8_t index, const String &arg) {
    if (index >= MAX_RELAYS || !s_relays[index].active) return;

    String cleanArg = arg;
    cleanArg.trim();
    cleanArg.toUpperCase();

    if (cleanArg == "1" || cleanArg == "ON") {
        digitalWrite(s_relays[index].pin, HIGH);
        LOG_INFO(s_relays[index].name + " set to ON");
    } else if (cleanArg == "0" || cleanArg == "OFF") {
        digitalWrite(s_relays[index].pin, LOW);
        LOG_INFO(s_relays[index].name + " set to OFF");
    } else {
        LOG_WARNING("Invalid argument for " + s_relays[index].name + ": " + arg);
    }
}

static void relay1_cmd(const String &arg) { relay_cmd_handler(0, arg); }
static void relay2_cmd(const String &arg) { relay_cmd_handler(1, arg); }
static void relay3_cmd(const String &arg) { relay_cmd_handler(2, arg); }

/**
 * @brief Driver entry point handling signals.
 */
bool Xdrv2(Signal_t signal) {
    switch (signal) {
        case SIG_INIT: {
            uint8_t used_count = 0;

            for (uint8_t i = 0; i < MAX_RELAYS; i++) {
                String relay_name = "Relay" + String(i + 1);
                
                if (is_use_name(relay_name.c_str())) {
                    int8_t pin = get_gpio_by_name(relay_name.c_str());
                    
                    if (pin != GPIO_INVALID) {
                        s_relays[i].pin = pin;
                        s_relays[i].active = true;
                        s_relays[i].name = relay_name;

                        pinMode(pin, OUTPUT);
                        digitalWrite(pin, LOW);

                        // Register exact command corresponding to the relay
                        if (i == 0) register_cmd("RELAY1", relay1_cmd);
                        else if (i == 1) register_cmd("RELAY2", relay2_cmd);
                        else if (i == 2) register_cmd("RELAY3", relay3_cmd);

                        used_count++;
                        LOG_INFO("Registered " + relay_name + " on GPIO " + String(pin));
                    } else {
                        s_relays[i].active = false;
                        LOG_ERROR("Failed to resolve GPIO for " + relay_name);
                    }
                } else {
                    s_relays[i].active = false;
                }
            }

            // Return true if at least 1 relay is initialized successfully
            if (used_count > 0) {
                LOG_INFO("[Xdrv2] Relay driver initialized with " + String(used_count) + " relay(s)");
                return true;
            }

            LOG_WARNING("[Xdrv2] No relays configured");
            return false;
        }
        case SIG_WEB_POLL: {
            for (uint8_t i = 0; i < MAX_RELAYS; i++) {
                if (s_relays[i].active) {
                    String state = digitalRead(s_relays[i].pin) ? "ON" : "OFF";
                    updateElementValue(s_relays[i].name, state);
                }
            }
            return true;
        }

        case SIG_MQTT_PUBLISH: {
            for (uint8_t i = 0; i < MAX_RELAYS; i++) {
                if (s_relays[i].active) {
                    const char* state = digitalRead(s_relays[i].pin) ? "ON" : "OFF";
                    mqtt_add_telemetry(s_relays[i].name.c_str(), state);
                }
            }
            return true;
        }
        default:
            return false;
    }
}
#endif //__USE_RELAY