#include <Arduino.h>
#include <Preferences.h>
#include "built_in.h"
#include "TienInterpreter.h"
#include "core/core_log.h"

#define NVS_NAMESPACE "tien_nvs"

/**
 * @brief Write data to NVS with polymorphism support
 * Supported types: int, float, bool, string
 */
static value_t *built_in_nvs_write(value_t **argv, int argc) {
    if (argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL || argv[0]->type != VAL_STRING) {
        ti_log("[ERROR] nvs_write: Expect (string key, value)\n");
        ti_fatal();
    }

    const char* key = argv[0]->string_val;
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) { // false = Read/Write mode
        LOG_ERROR_STR("nvs_write: Failed to open NVS namespace '%s'", NVS_NAMESPACE);
        return val_new_bool(false);
    }
    bool success = false;

    switch (argv[1]->type) {
        case VAL_INT:
            success = (prefs.putInt(key, argv[1]->int_val) != 0);
            break;
        case VAL_FLOAT:
            success = (prefs.putFloat(key, argv[1]->float_val) != 0);
            break;
        case VAL_BOOL:
            success = (prefs.putBool(key, argv[1]->bool_val) != 0);
            break;
        case VAL_STRING:
            success = (prefs.putString(key, argv[1]->string_val ? argv[1]->string_val : "") != 0);
            break;
        default:
            ti_log("[ERROR] nvs_write: Unsupported data type. Only int, float, bool, string are allowed.\n");
            prefs.end();
            ti_fatal();
    }

    if (!success) {
        LOG_ERROR_STR("nvs_write: Failed to write key '%s' to NVS namespace '%s'", key, NVS_NAMESPACE);
    }

    prefs.end();
    return val_new_bool(success);
}

/**
 * @brief Read data from NVS with polymorphism.
 * Return type is automatically inferred based on default_value type.
 */
static value_t *built_in_nvs_read(value_t **argv, int argc) {
    if (argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL || argv[0]->type != VAL_STRING) {
        ti_log("[ERROR] nvs_read: Expect (string key, default_value)\n");
        ti_fatal();
    }

    const char* key = argv[0]->string_val;
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) { // true = Read-only mode
        LOG_ERROR_STR("nvs_read: Failed to open NVS namespace '%s' in read-only mode", NVS_NAMESPACE);
    }
    value_t *ret = NULL;

    switch (argv[1]->type) {
        case VAL_INT:
            ret = val_new_int(prefs.getInt(key, argv[1]->int_val));
            break;
        case VAL_FLOAT:
            ret = val_new_float(prefs.getFloat(key, argv[1]->float_val));
            break;
        case VAL_BOOL:
            ret = val_new_bool(prefs.getBool(key, argv[1]->bool_val));
            break;
        case VAL_STRING: {
            String def = argv[1]->string_val ? argv[1]->string_val : "";
            String val = prefs.getString(key, def);
            ret = val_new_string(val.c_str());
            break;
        }
        default:
            ti_log("[ERROR] nvs_read: Unsupported default value type.\n");
            prefs.end();
            ti_fatal();
    }

    prefs.end();
    return ret;
}

void builtin_nvs_init(void) {
    /* Use NULL param and argc = -1 to bypass static type checking, enabling runtime polymorphism */
    register_builtin_function("nvs_write", VAL_BOOL, NULL, -1, built_in_nvs_write);
    register_builtin_function("nvs_read", VAL_NULL, NULL, -1, built_in_nvs_read);
}
