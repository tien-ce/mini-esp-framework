/**
 * @file builtin_script.cpp
 * @brief Built-in script execution function for TienInterpreter engine.
 */

#include "scripting/builtins/built_in.h"
#include "TienInterpreter.h"
#include "core/core_log.h"
#include "scripting/ti_interpreter.h"
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
static value_t *built_in_run_script(ti_handle_t handle, value_t **argv, int argc) {
    if (argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL ||
        argv[0]->type != VAL_STRING || argv[1]->type != VAL_STRING ||
        argv[0]->string_val == NULL || argv[1]->string_val == NULL) {
        ti_raise_error(handle, TI_ERR_INVALID_ARG, "%s: Expect 2 string arguments (name, code)", BUILTIN_RUN_SCRIPT);
        return NULL;
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
