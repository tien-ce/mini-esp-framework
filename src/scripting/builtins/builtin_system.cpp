#include "built_in.h"
#include "built_in.h"
#include "TienInterpreter.h"
#include "core/core_log.h"
#include "core/core_engine.h"
#include "core/info.h"
#include "services/wifi/wifi_task.h"
#include <Arduino.h>

static value_t *built_in_is_none(ti_handle_t handle, value_t **argv, int argc)
{
    if (argc != 1 || argv == NULL) {
        ti_log("[ERROR] %s: Expect 1 variable \n", BUILTIN_IS_NONE);
        ti_fatal();
    }
    if (argv[0] == NULL || argv[0]->type == VAL_NULL)
    {
      return val_new_bool(true);
    }
    else
    {
      return val_new_bool(false);
    }
}

/** @brief Built-in delay function to pause script execution for N milliseconds. */
static value_t *built_in_delay(ti_handle_t handle, value_t **argv, int argc) {
    if (argc != 1 || argv == NULL || argv[0] == NULL) {
        ti_log("[ERROR] %s: Invalid argument count, expect 1\n", BUILTIN_DELAY);
        ti_fatal();
    }
    // ti_wait releases the runtime lock while sleeping, so events, dispatcher handlers and ti_call callers keep running.
    // Its status (TI_ERR_INTERRUPTED when the script is stopped, stale handle) needs no handling: the script ends by itself.
    switch (argv[0]->type) {
        case VAL_INT:
            if (argv[0]->int_val > 0) {
                ti_wait(handle, (uint32_t)argv[0]->int_val);
            }
            break;
        case VAL_FLOAT:
            if (argv[0]->float_val >= 1.0f) {
                ti_wait(handle, (uint32_t)argv[0]->float_val);
            }
            break;
        default:
            ti_log("[ERROR] %s: Unexpected type %d\n", BUILTIN_DELAY, argv[0]->type);
            ti_fatal();
            break;
    }
    return val_new_null();
}

/**
 * @brief Stores a dict entry; val_dict_set takes ownership of the value even on failure.
 * @param dict Target dict value.
 * @param key Entry key.
 * @param val Entry value (owned by the dict afterwards).
 */
static void infoSet(value_t *dict, const char *key, value_t *val) {
    if (val == NULL) return;
    val_dict_set(dict->dict_val, key, val);
}

/** @brief Built-in get_device_ip(): current IP as string, "" when the network is not connected. */
static value_t *built_in_get_device_ip(ti_handle_t handle, value_t **argv, int argc) {
    // Step 1: ask the Wi-Fi service (it already hides the "no lease" case)
    String ip = get_wifi_local_ip();
    return val_new_string(ip.c_str());
}

/** @brief Built-in get_device_info(): dict with network, identity and runtime information. */
static value_t *built_in_get_device_info(ti_handle_t handle, value_t **argv, int argc) {
    value_t *info = val_new_dict();
    if (info == NULL) {
        ti_raise_error(handle, TI_ERR_INVALID_ARG, "get_device_info() out of memory");
        return NULL;
    }

    // Step 1: network information from the Wi-Fi service
    String ip = get_wifi_local_ip();
    bool connected = is_wifi_connected();
    infoSet(info, "ip", val_new_string(ip.c_str()));
    infoSet(info, "hostname", val_new_string(getWifiClientID().c_str()));
    infoSet(info, "ssid", val_new_string(getWifiSSID().c_str()));
    infoSet(info, "wifi_connected", val_new_bool(connected));
    infoSet(info, "rssi", val_new_int(connected ? get_wifi_rssi() : 0));
    infoSet(info, "mqtt_connected", val_new_bool(CoreState_GetMqtt() == MQTT_STATE_CONNECTED));

    // Step 2: chip and firmware information from the info provider
    infoSet(info, "mac", val_new_string(esp_info_get_mac_str()));
    infoSet(info, "model", val_new_string(esp_info_get_model()));
    infoSet(info, "fw_version", val_new_string(esp_info_get_fw_version()));
    infoSet(info, "uptime", val_new_int((int)esp_info_get_uptime_sec()));
    infoSet(info, "free_heap", val_new_int((int)esp_info_get_free_heap()));
    return info;
}

void builtin_system_init(void) {
    static param_t delay_params[] = { { VAL_INT, (char*)"ms" } };
    register_builtin_function(BUILTIN_GET_DEVICE_IP, VAL_STRING, NULL, 0, built_in_get_device_ip);
    register_builtin_function(BUILTIN_GET_DEVICE_INFO, VAL_DICT, NULL, 0, built_in_get_device_info);
    register_builtin_function(BUILTIN_IS_NONE, VAL_BOOL, NULL, -1, built_in_is_none);
    register_builtin_function(BUILTIN_DELAY, VAL_VOID, delay_params, 1, built_in_delay);
}
