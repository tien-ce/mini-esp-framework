#include <Arduino.h>
#include "built_in.h"
#include "TienInterpreter.h"
#include "services/mqtt/mqtt_task.h"

/**
 * @brief Add a telemetry key-value pair to the MQTT payload.
 * @param argv Arguments: (string key, int/float/bool/string value).
 * @param argc Argument count (must be 2).
 * @return Boolean value_t: true if the pair was stored, false otherwise.
 */
static value_t *built_in_mqtt_update(ti_handle_t handle, value_t **argv, int argc) {
    /* Step 1: Validate arguments */
    if (argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL ||
        argv[0]->type != VAL_STRING || argv[0]->string_val == NULL) {
        ti_log("[ERROR] mqtt_update: Expect (string key, value)\n");
        ti_fatal();
    }

    String key = argv[0]->string_val;
    bool ok = false;

    /* Step 2: Dispatch on the runtime type of the value */
    switch (argv[1]->type) {
        case VAL_INT:
            ok = mqtt_add_telemetry(key, (int)argv[1]->int_val);
            break;
        case VAL_FLOAT:
            ok = mqtt_add_telemetry(key, (float)argv[1]->float_val);
            break;
        case VAL_BOOL:
            ok = mqtt_add_telemetry(key, (bool)argv[1]->bool_val);
            break;
        case VAL_STRING:
            ok = mqtt_add_telemetry(key, argv[1]->string_val ? (const char *)argv[1]->string_val : "");
            break;
        default:
            ti_log("[ERROR] mqtt_update: Unsupported value type. Only int, float, bool, string are allowed.\n");
            ti_fatal();
    }

    return val_new_bool(ok);
}

/**
 * @brief Set an attribute (published retained on IOT-MONITOR/<ID>/ATTR, only when it changes).
 * @param argv Arguments: (string key, int/float/bool/string value).
 * @param argc Argument count (must be 2).
 * @return Boolean value_t: true if stored, false otherwise.
 */
static value_t *built_in_mqtt_set_attribute(ti_handle_t handle, value_t **argv, int argc) {
    /* Step 1: Validate arguments */
    if (argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL ||
        argv[0]->type != VAL_STRING || argv[0]->string_val == NULL) {
        ti_log("[ERROR] mqtt_set_attribute: Expect (string key, value)\n");
        ti_fatal();
    }

    String key = argv[0]->string_val;
    bool ok = false;

    /* Step 2: Dispatch on the runtime type of the value */
    switch (argv[1]->type) {
        case VAL_INT:
            ok = mqtt_set_attribute(key, (int)argv[1]->int_val);
            break;
        case VAL_FLOAT:
            ok = mqtt_set_attribute(key, (float)argv[1]->float_val);
            break;
        case VAL_BOOL:
            ok = mqtt_set_attribute(key, (bool)argv[1]->bool_val);
            break;
        case VAL_STRING:
            ok = mqtt_set_attribute(key, argv[1]->string_val ? (const char *)argv[1]->string_val : "");
            break;
        default:
            ti_log("[ERROR] mqtt_set_attribute: Unsupported value type. Only int, float, bool, string are allowed.\n");
            ti_fatal();
    }

    return val_new_bool(ok);
}

/**
 * @brief Send an alarm to the central monitor (wraps mqtt_alarm_publish()).
 * @param argv Arguments: (string code, string msg, string source, string level, bool active).
 * @param argc Argument count (must be 5).
 * @return Boolean value_t: true (OK) when queued, false (ERROR) otherwise.
 */
static value_t *built_in_mqtt_alarm(ti_handle_t handle, value_t **argv, int argc) {
    /* Step 1: Validate arguments: four strings and a bool */
    bool valid = (argc == 5 && argv != NULL);
    for (int i = 0; valid && i < 4; i++) {
        valid = argv[i] != NULL && argv[i]->type == VAL_STRING;
    }
    valid = valid && argv[4] != NULL && argv[4]->type == VAL_BOOL;
    if (!valid) {
        ti_log("[ERROR] mqtt_alarm: Expect (string code, string msg, string source, string level, bool active)\n");
        ti_fatal();
    }

    /* Step 2: Queue it; the MQTT task publishes it */
    bool ok = mqtt_alarm_publish(argv[0]->string_val ? (const char *)argv[0]->string_val : "",
                                 argv[1]->string_val ? (const char *)argv[1]->string_val : "",
                                 argv[2]->string_val ? (const char *)argv[2]->string_val : "",
                                 argv[3]->string_val ? (const char *)argv[3]->string_val : "",
                                 argv[4]->bool_val);
    return val_new_bool(ok);
}

void builtin_mqtt_init(void) {
    /* NULL params and argc = -1 bypass static type checking to allow runtime polymorphism */
    register_builtin_function("mqtt_update", VAL_BOOL, NULL, -1, built_in_mqtt_update);
    register_builtin_function("mqtt_alarm", VAL_BOOL, NULL, -1, built_in_mqtt_alarm);
    register_builtin_function("mqtt_set_attribute", VAL_BOOL, NULL, -1, built_in_mqtt_set_attribute);
}
