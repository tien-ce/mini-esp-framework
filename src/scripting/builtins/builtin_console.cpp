#include "built_in.h"
#include "TienInterpreter.h"
#include "core/core_log.h"
#include <Arduino.h>

/**
 * @brief Helper function to format arguments into a single String buffer.
 * Reuses identical formatting logic across print, print_error, print_warning, and print_debug.
 */
static String format_log_args(const char *fn_name, value_t **argv, int argc) {
    if (argc == 0 || argv == NULL) {
        return "";
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
                ti_log("[ERROR] %s: Unexpected type %d\n", fn_name, argv[i]->type);
                ti_fatal();
                break;
        }
    }
    return buffer;
}

/** @brief Generic dispatcher for console logging builtins. */
static value_t *built_in_log_generic(const char *fn_name, LogLevel level, value_t **argv, int argc) {
    String msg = format_log_args(fn_name, argv, argc);
    core_log_print(msg, level);
    return val_new_null();
}

/** @brief Built-in print / print_info callback. */
static value_t *built_in_print(ti_handle_t handle, value_t **argv, int argc) {
    (void)handle;
    return built_in_log_generic(BUILTIN_PRINT, LOG_LEVEL_INFO, argv, argc);
}

/** @brief Built-in print_error callback. */
static value_t *built_in_print_error(ti_handle_t handle, value_t **argv, int argc) {
    (void)handle;
    return built_in_log_generic(BUILTIN_PRINT_ERROR, LOG_LEVEL_ERROR, argv, argc);
}

/** @brief Built-in print_warning callback. */
static value_t *built_in_print_warning(ti_handle_t handle, value_t **argv, int argc) {
    (void)handle;
    return built_in_log_generic(BUILTIN_PRINT_WARNING, LOG_LEVEL_WARNING, argv, argc);
}

/** @brief Built-in print_debug callback. */
static value_t *built_in_print_debug(ti_handle_t handle, value_t **argv, int argc) {
    (void)handle;
    return built_in_log_generic(BUILTIN_PRINT_DEBUG, LOG_LEVEL_DEBUG, argv, argc);
}

void builtin_console_init(void) {
    register_builtin_function(BUILTIN_PRINT, VAL_VOID, NULL, -1, built_in_print);
    register_builtin_function(BUILTIN_PRINT_INFO, VAL_VOID, NULL, -1, built_in_print);
    register_builtin_function(BUILTIN_PRINT_ERROR, VAL_VOID, NULL, -1, built_in_print_error);
    register_builtin_function(BUILTIN_PRINT_WARNING, VAL_VOID, NULL, -1, built_in_print_warning);
    register_builtin_function(BUILTIN_PRINT_DEBUG, VAL_VOID, NULL, -1, built_in_print_debug);
}
