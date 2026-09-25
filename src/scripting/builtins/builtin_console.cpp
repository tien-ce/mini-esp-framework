#include "built_in.h"
#include "built_in.h"
#include "TienInterpreter.h"
#include "core/core_log.h"
#include <Arduino.h>

/** @brief Built-in print function callback registered into TienInterpreter engine. */
static value_t *built_in_print(value_t **argv, int argc) {
    if (argc == 0) {
        LOG_INFO("");
        return val_new_null();
    }
    if (argv == NULL) {
        return val_new_null();
    }
    String buffer = "";
    for (int i = 0; i < argc; i++) {
        if (argv[i] == NULL) {
            buffer += "null";
            continue;
        }
        switch (argv[i]->type) {
            case VAL_STRING:
                buffer += (argv[i]->string_val ? String(argv[i]->string_val) : "null");
                break;
            case VAL_INT:
                buffer += String(argv[i]->int_val);
                break;
            case VAL_FLOAT:
                buffer += String(argv[i]->float_val, 2);
                break;
            case VAL_BOOL:
                buffer += (argv[i]->bool_val ? "true" : "false");
                break;
            case VAL_NULL:
                buffer += "null";
                break;
            default:
                ti_log("[ERROR] %s: Unexpected type %d\n", BUILTIN_PRINT, argv[i]->type);
                ti_fatal();
                break;
        }
    }
    LOG_INFO(buffer);
    return val_new_null();
}

void builtin_console_init(void) {
    register_builtin_function(BUILTIN_PRINT, VAL_VOID, NULL, -1, built_in_print);
}
