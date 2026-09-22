#include "built_in.h"
#include "built_in.h"
#include "TienInterpreter.h"
#include "core/log_task.h"
#include <ArduinoJson.h>

/** @brief Static helper to validate arguments, parse JSON string, and extract a JsonVariant for a given key. */
static bool parse_json_and_get_element(value_t **argv, int argc, const char *func_name, JsonDocument &doc, JsonVariant &out_variant) {
    if (argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL || argv[0]->type != VAL_STRING || argv[1]->type != VAL_STRING) {
        ti_log("[ERROR] %s: Expect 2 string arguments (json_string, key)\n", func_name);
        ti_fatal();
    }

    const char *json_str = argv[0]->string_val;
    const char *key = argv[1]->string_val;

    if (json_str == NULL || key == NULL) {
        return false;
    }

    DeserializationError err = deserializeJson(doc, json_str);
    if (err) {
        LOG_WARNING(String(func_name) + " parse error: " + String(err.c_str()));
        return false;
    }

    if (!doc.containsKey(key) || doc[key].isNull()) {
        return false;
    }

    out_variant = doc[key];
    return true;
}

/** @brief Built-in get_json function to extract an element from JSON string with its native type. */
static value_t *built_in_get_json(value_t **argv, int argc) {
    JsonDocument doc;
    JsonVariant val;
    if (!parse_json_and_get_element(argv, argc, BUILTIN_GET_JSON, doc, val)) {
        return val_new_null();
    }

    if (val.isNull()) {
        return val_new_null();
    }
    if (val.is<bool>()) {
        return val_new_bool(val.as<bool>());
    }
    if (val.is<int>()) {
        return val_new_int(val.as<int>());
    }
    if (val.is<float>()) {
        return val_new_float(val.as<float>());
    }
    if (val.is<const char*>()) {
        return val_new_string(val.as<const char*>());
    }
    if (val.is<JsonObject>() || val.is<JsonArray>()) {
        String json_out;
        serializeJson(val, json_out);
        return val_new_string(json_out.c_str());
    }

    String val_str = val.as<String>();
    return val_new_string(val_str.c_str());
}

/** @brief Built-in get_json_as_string function to extract an element from JSON string as a string. */
static value_t *built_in_get_json_as_string(value_t **argv, int argc) {
    JsonDocument doc;
    JsonVariant val;
    if (!parse_json_and_get_element(argv, argc, BUILTIN_GET_JSON_AS_STRING, doc, val)) {
        return val_new_null();
    }

    if (val.is<const char*>()) {
        return val_new_string(val.as<const char*>());
    }

    if (val.is<JsonObject>() || val.is<JsonArray>()) {
        String json_out;
        serializeJson(val, json_out);
        return val_new_string(json_out.c_str());
    }

    String val_str = val.as<String>();
    return val_new_string(val_str.c_str());
}

/** @brief Built-in get_json_as_int function to extract an element from JSON string as an integer. */
static value_t *built_in_get_json_as_int(value_t **argv, int argc) {
    JsonDocument doc;
    JsonVariant val;
    if (!parse_json_and_get_element(argv, argc, BUILTIN_GET_JSON_AS_INT, doc, val)) {
        return val_new_null();
    }

    return val_new_int(val.as<int>());
}

/** @brief Built-in get_json_as_float function to extract an element from JSON string as a float. */
static value_t *built_in_get_json_as_float(value_t **argv, int argc) {
    JsonDocument doc;
    JsonVariant val;
    if (!parse_json_and_get_element(argv, argc, BUILTIN_GET_JSON_AS_FLOAT, doc, val)) {
        return val_new_null();
    }

    return val_new_float(val.as<float>());
}

/** @brief Built-in get_json_as_bool function to extract an element from JSON string as a boolean. */
static value_t *built_in_get_json_as_bool(value_t **argv, int argc) {
    JsonDocument doc;
    JsonVariant val;
    if (!parse_json_and_get_element(argv, argc, BUILTIN_GET_JSON_AS_BOOL, doc, val)) {
        return val_new_null();
    }

    return val_new_bool(val.as<bool>());
}

void builtin_json_init(void) {
    static param_t json_params[] = { { VAL_STRING, (char*)"json" }, { VAL_STRING, (char*)"key" } };
    register_builtin_function(BUILTIN_GET_JSON, VAL_STRING, json_params, 2, built_in_get_json);
    register_builtin_function(BUILTIN_GET_JSON_AS_STRING, VAL_STRING, json_params, 2, built_in_get_json_as_string);
    register_builtin_function(BUILTIN_GET_JSON_AS_INT, VAL_INT, json_params, 2, built_in_get_json_as_int);
    register_builtin_function(BUILTIN_GET_JSON_AS_FLOAT, VAL_FLOAT, json_params, 2, built_in_get_json_as_float);
    register_builtin_function(BUILTIN_GET_JSON_AS_BOOL, VAL_BOOL, json_params, 2, built_in_get_json_as_bool);
}
