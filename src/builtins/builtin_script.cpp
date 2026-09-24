/**
 * @file builtin_script.cpp
 * @brief Built-in script execution function for TienInterpreter engine.
 */

#include "built_in.h"
#include "TienInterpreter.h"
#include "core/log_task.h"
#include "core/ti_interpreter.h"
#include <Arduino.h>

/**
 * @brief Built-in run_script function to execute a Tien script directly from source code.
 *
 * Accepts 2 string parameters:
 *   - argv[0]: script name (const char *name)
 *   - argv[1]: script source code string (const char *source_code)
 *
 * @param argv Array of argument value pointers.
 * @param argc Number of arguments passed.
 * @return value_t* Boolean true on successful invocation.
 */
static value_t *built_in_run_script(value_t **argv, int argc) {
    if (argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL ||
        argv[0]->type != VAL_STRING || argv[1]->type != VAL_STRING ||
        argv[0]->string_val == NULL || argv[1]->string_val == NULL) {
        LOG_ERROR(String(BUILTIN_RUN_SCRIPT) + ": Expect 2 string arguments (name, code)");
        ti_log("[ERROR] %s: Expect 2 string arguments (name, code)\n", BUILTIN_RUN_SCRIPT);
        ti_fatal();
        return val_new_null();
    }

    const char *name = argv[0]->string_val;
    const char *source_code = argv[1]->string_val;

    tien_run_script(name, source_code);
    return val_new_bool(true);
}

/**
 * @brief Initializes and registers the builtin script execution function and aliases.
 */
void builtin_script_init(void) {
    static param_t run_script_params[] = { { VAL_STRING, (char*)"name" }, { VAL_STRING, (char*)"code" } };

    register_builtin_function(BUILTIN_RUN_SCRIPT, VAL_BOOL, run_script_params, 2, built_in_run_script);
    register_builtin_function("script_run", VAL_BOOL, run_script_params, 2, built_in_run_script);
}
