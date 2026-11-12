#include <Arduino.h>
#include "scripting/builtins/built_in.h"
#include "TienInterpreter.h"
#include "core/core_log.h"
#include "services/web/web_template.h"
#include "core/core_engine.h"

/**
 * @brief Convert a scalar value (int, float, bool, string) to the text shown on the Web UI.
 * @param[in]  v   Value to convert.
 * @param[out] out Receives the text.
 * @return true on success, false when the value is a container (dict, list, bytes, handle, ...).
 */
static bool web_value_to_str(const value_t *v, String &out) {
    switch (v->type) {
        case VAL_INT:    out = String(v->int_val);                                  return true;
        case VAL_FLOAT:  out = String(v->float_val);                                return true;
        case VAL_BOOL:   out = v->bool_val ? "true" : "false";                      return true;
        case VAL_STRING: out = v->string_val ? String(v->string_val) : String(""); return true;
        default:         return false;
    }
}

/**
 * @brief val_dict_foreach callback: update one element per entry, skip container values.
 */
static bool web_ui_update_entry(const char *key, value_t *value, void *user_context) {
    String value_str;
    if (web_value_to_str(value, value_str)) {
        updateElementValue(String(key), value_str);
    }
    return true;   /* always go on with the next entry */
}

/**
 * @brief Update the display value on the Web Template UI.
 * @details Two forms:
 *          - web_ui_update(dict values): every key is an element id; container values are skipped.
 *          - web_ui_update(string element_id, value): value is int, float, bool or string.
 */
static value_t *built_in_web_ui_update(ti_handle_t handle, value_t **argv, int argc) {
    /* Form 1: one dict, update every entry */
    if (argc == 1 && argv[0]->type == VAL_DICT) {
        val_dict_foreach(argv[0]->dict_val, web_ui_update_entry, NULL);
        return val_new_bool(true);
    }

    /* Form 2: (string element_id, value) */
    if (argc != 2) {
        ti_raise_error(handle, TI_ERR_INVALID_ARG, "web_ui_update: expect (dict values) or (string element_id, value), got %d args", argc);
        return NULL;
    }

    if (argv[0]->type != VAL_STRING) {
        ti_raise_error(handle, TI_ERR_TYPE_MISMATCH, "web_ui_update: element_id must be a string");
        return NULL;
    }

    String element_id = argv[0]->string_val ? argv[0]->string_val : "";
    String value_str;
    if (!web_value_to_str(argv[1], value_str)) {
        ti_raise_error(handle, TI_ERR_TYPE_MISMATCH, "web_ui_update: value must be int, float, bool or string");
        return NULL;
    }

    updateElementValue(element_id, value_str);
    return val_new_bool(true);
}

void builtin_web_init(void) {
    /* 
     * Set param_count = -1 and params = NULL to bypass static type checking,
     * allowing the second parameter (value) to accept any primitive data type (Polymorphism)
     * strictly adhering to the TI_INTERPRETER_GUIDE.md documentation.
     */
    register_builtin_function("web_ui_update", VAL_BOOL, NULL, -1, built_in_web_ui_update);
}
