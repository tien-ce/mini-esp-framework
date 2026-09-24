#include <Arduino.h>
#include "built_in.h"
#include "TienInterpreter.h"
#include "core/log_task.h"
#include "core/web/web_template.h"

/**
 * @brief Update the display value on the Web Template UI.
 * Polymorphism support: automatically converts int, float, bool, string to C++ String 
 * before calling the system function updateElementValue.
 */
static value_t *built_in_web_ui_update(value_t **argv, int argc) {
    if (argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL) {
        ti_log("[ERROR] web_ui_update: Expect 2 args (string element_id, value)\n");
        ti_fatal();
    }
    
    if (argv[0]->type != VAL_STRING) {
        ti_log("[ERROR] web_ui_update: element_id must be a string\n");
        ti_fatal();
    }

    String element_id = argv[0]->string_val ? argv[0]->string_val : "";
    String value_str = "";

    switch (argv[1]->type) {
        case VAL_INT:
            value_str = String(argv[1]->int_val);
            break;
        case VAL_FLOAT:
            value_str = String(argv[1]->float_val);
            break;
        case VAL_BOOL:
            value_str = argv[1]->bool_val ? "true" : "false";
            break;
        case VAL_STRING:
            value_str = argv[1]->string_val ? String(argv[1]->string_val) : "";
            break;
        default:
            ti_log("[ERROR] web_ui_update: Unsupported value type. Allowed: int, float, bool, string\n");
            ti_fatal();
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
