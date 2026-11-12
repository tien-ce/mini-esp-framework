#include <Arduino.h>
#include "scripting/builtins/built_in.h"
#include "TienInterpreter.h"
#include "services/mqtt/mqtt_task.h"

/* -------------------------------------------------------------------------- */
/*                                   HELPERS                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Add one telemetry key-value pair to the MQTT payload.
 * @param[in]  key   Telemetry key.
 * @param[in]  v     Value: int, float, bool or string.
 * @param[out] ok    Receives the result of mqtt_add_telemetry().
 * @return true when the type is supported, false when the value is a container (dict, list, bytes, handle, ...).
 */
static bool mqtt_put_telemetry(const String &key, const value_t *v, bool *ok) {
    switch (v->type) {
        case VAL_INT:    *ok = mqtt_add_telemetry(key, (int)v->int_val);                                 return true;
        case VAL_FLOAT:  *ok = mqtt_add_telemetry(key, (float)v->float_val);                             return true;
        case VAL_BOOL:   *ok = mqtt_add_telemetry(key, (bool)v->bool_val);                               return true;
        case VAL_STRING: *ok = mqtt_add_telemetry(key, v->string_val ? (const char *)v->string_val : ""); return true;
        default:         return false;
    }
}

/**
 * @brief Set one attribute (published retained on IOT-MONITOR/<ID>/ATTR, only when it changes).
 * @param[in]  key   Attribute key.
 * @param[in]  v     Value: int, float, bool or string.
 * @param[out] ok    Receives the result of mqtt_set_attribute().
 * @return true when the type is supported, false when the value is a container (dict, list, bytes, handle, ...).
 */
static bool mqtt_put_attribute(const String &key, const value_t *v, bool *ok) {
    switch (v->type) {
        case VAL_INT:    *ok = mqtt_set_attribute(key, (int)v->int_val);                                 return true;
        case VAL_FLOAT:  *ok = mqtt_set_attribute(key, (float)v->float_val);                             return true;
        case VAL_BOOL:   *ok = mqtt_set_attribute(key, (bool)v->bool_val);                               return true;
        case VAL_STRING: *ok = mqtt_set_attribute(key, v->string_val ? (const char *)v->string_val : ""); return true;
        default:         return false;
    }
}

/**
 * @brief val_dict_foreach callback for mqtt_update(dict): skip container values.
 * @param user_context bool*, cleared when one pair is not stored.
 */
static bool mqtt_telemetry_entry(const char *key, value_t *value, void *user_context) {
    bool ok = true;
    if (mqtt_put_telemetry(String(key), value, &ok) && !ok) {
        *(bool *)user_context = false;
    }
    return true;   /* always go on with the next entry */
}

/**
 * @brief val_dict_foreach callback for mqtt_set_attribute(dict): skip container values.
 * @param user_context bool*, cleared when one pair is not stored.
 */
static bool mqtt_attribute_entry(const char *key, value_t *value, void *user_context) {
    bool ok = true;
    if (mqtt_put_attribute(String(key), value, &ok) && !ok) {
        *(bool *)user_context = false;
    }
    return true;   /* always go on with the next entry */
}

/* -------------------------------------------------------------------------- */
/*                                  BUILTINS                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Add telemetry to the MQTT payload.
 * @details Two forms:
 *          - mqtt_update(dict values): every entry is a pair; container values are skipped.
 *          - mqtt_update(string key, value): value is int, float, bool or string.
 * @return Boolean value_t: true if every pair was stored, false otherwise.
 */
static value_t *built_in_mqtt_update(ti_handle_t handle, value_t **argv, int argc) {
    /* Form 1: one dict, add every entry */
    if (argc == 1 && argv[0]->type == VAL_DICT) {
        bool all_ok = true;
        val_dict_foreach(argv[0]->dict_val, mqtt_telemetry_entry, &all_ok);
        return val_new_bool(all_ok);
    }

    /* Form 2: (string key, value) */
    if (argc != 2) {
        ti_raise_error(handle, TI_ERR_INVALID_ARG, "mqtt_update: expect (dict values) or (string key, value), got %d args", argc);
        return NULL;
    }
    if (argv[0]->type != VAL_STRING) {
        ti_raise_error(handle, TI_ERR_TYPE_MISMATCH, "mqtt_update: key must be a string");
        return NULL;
    }

    bool ok = false;
    if (!mqtt_put_telemetry(String(argv[0]->string_val), argv[1], &ok)) {
        ti_raise_error(handle, TI_ERR_TYPE_MISMATCH, "mqtt_update: value must be int, float, bool or string");
        return NULL;
    }
    return val_new_bool(ok);
}

/**
 * @brief Set attributes (published retained on IOT-MONITOR/<ID>/ATTR, only when they change).
 * @details Two forms:
 *          - mqtt_set_attribute(dict values): every entry is a pair; container values are skipped.
 *          - mqtt_set_attribute(string key, value): value is int, float, bool or string.
 * @return Boolean value_t: true if every pair was stored, false otherwise.
 */
static value_t *built_in_mqtt_set_attribute(ti_handle_t handle, value_t **argv, int argc) {
    /* Form 1: one dict, set every entry */
    if (argc == 1 && argv[0]->type == VAL_DICT) {
        bool all_ok = true;
        val_dict_foreach(argv[0]->dict_val, mqtt_attribute_entry, &all_ok);
        return val_new_bool(all_ok);
    }

    /* Form 2: (string key, value) */
    if (argc != 2) {
        ti_raise_error(handle, TI_ERR_INVALID_ARG, "mqtt_set_attribute: expect (dict values) or (string key, value), got %d args", argc);
        return NULL;
    }
    if (argv[0]->type != VAL_STRING) {
        ti_raise_error(handle, TI_ERR_TYPE_MISMATCH, "mqtt_set_attribute: key must be a string");
        return NULL;
    }

    bool ok = false;
    if (!mqtt_put_attribute(String(argv[0]->string_val), argv[1], &ok)) {
        ti_raise_error(handle, TI_ERR_TYPE_MISMATCH, "mqtt_set_attribute: value must be int, float, bool or string");
        return NULL;
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
        ti_raise_error(handle, TI_ERR_INVALID_ARG, "mqtt_alarm: expect (string code, string msg, string source, string level, bool active)");
        return NULL;
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
