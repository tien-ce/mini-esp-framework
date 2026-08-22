#include "config.h"
#ifdef USE_RELAY
#include <Arduino.h>
#include "core/core_engine.h"
#include "built_in.h"
#include "TienInterpreter.h"

#define MAX_RELAYS 3

typedef struct {
    int8_t pin;
    bool active;
    String name;
} RelayConfig_t;

static RelayConfig_t s_relays[MAX_RELAYS];

/* -------------------------------------------------------------------------- */
/*                        INTERPRETER BUILT-IN FUNCTIONS                      */
/* -------------------------------------------------------------------------- */

/** @brief Built-in relay_get_state function returning relay state (1 for ON, 0 for OFF). */
static value_t *built_in_relay_get_state(value_t **argv, int argc) {
    if (argc != 1 || argv == NULL || argv[0] == NULL || argv[0]->type != VAL_INT) {
        ti_log("[ERROR] %s: Expect 1 integer argument (relay_number 1-%d)\n", BUILTIN_RELAY_GET_STATE, MAX_RELAYS);
        ti_fatal();
    }
    int relay_num = argv[0]->int_val;
    int idx = -1;
    if (relay_num >= 1 && relay_num <= MAX_RELAYS) {
        idx = relay_num - 1;
    } else if (relay_num == 0) {
        idx = 0;
    } else {
        ti_log("[ERROR] %s: Relay number %d out of range (1-%d)\n", BUILTIN_RELAY_GET_STATE, relay_num, MAX_RELAYS);
        ti_fatal();
    }

    if (!s_relays[idx].active) {
        LOG_WARNING("Relay" + String(idx + 1) + " is not active/configured");
        return val_new_int(0);
    }

    int state = (digitalRead(s_relays[idx].pin) == HIGH) ? 1 : 0;
    return val_new_int(state);
}

/** @brief Built-in relay_set_state function setting relay state (accepts int, bool, or string ON/OFF). */
static value_t *built_in_relay_set_state(value_t **argv, int argc) {
    if (argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL || argv[0]->type != VAL_INT) {
        ti_log("[ERROR] %s: Expect 2 arguments (relay_number, state)\n", BUILTIN_RELAY_SET_STATE);
        ti_fatal();
    }
    int relay_num = argv[0]->int_val;
    int idx = -1;
    if (relay_num >= 1 && relay_num <= MAX_RELAYS) {
        idx = relay_num - 1;
    } else if (relay_num == 0) {
        idx = 0;
    } else {
        ti_log("[ERROR] %s: Relay number %d out of range (1-%d)\n", BUILTIN_RELAY_SET_STATE, relay_num, MAX_RELAYS);
        ti_fatal();
    }

    if (!s_relays[idx].active) {
        LOG_WARNING("Relay" + String(idx + 1) + " is not active/configured");
        return val_new_bool(false);
    }

    bool targetState = false;
    if (argv[1]->type == VAL_INT) {
        targetState = (argv[1]->int_val != 0);
    } else if (argv[1]->type == VAL_BOOL) {
        targetState = argv[1]->bool_val;
    } else if (argv[1]->type == VAL_STRING && argv[1]->string_val != NULL) {
        String s = argv[1]->string_val;
        s.trim();
        s.toUpperCase();
        targetState = (s == "1" || s == "ON" || s == "TRUE" || s == "HIGH");
    } else {
        ti_log("[ERROR] %s: Invalid state argument type %d\n", BUILTIN_RELAY_SET_STATE, argv[1]->type);
        ti_fatal();
    }

    digitalWrite(s_relays[idx].pin, targetState ? HIGH : LOW);
    LOG_INFO(s_relays[idx].name + " set to " + (targetState ? "ON" : "OFF"));
    return val_new_bool(true);
}

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
            register_builtin_function(BUILTIN_RELAY_GET_STATE, built_in_relay_get_state);
            register_builtin_function(BUILTIN_RELAY_SET_STATE, built_in_relay_set_state);

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