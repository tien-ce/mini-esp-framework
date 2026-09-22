#include "built_in.h"
#include "built_in.h"
#include "TienInterpreter.h"
#include "core/log_task.h"
#include <Arduino.h>

static value_t *built_in_is_none(value_t **argv, int argc)
{
    if (argc != 1 || argv == NULL) {
        ti_log("[ERROR] %s: Expect 1 variable \n", BUILTIN_IS_NONE);
        ti_fatal();
    }
    if (argv[0] == NULL || argv[0]->type == VAL_NULL)
    {
      return val_new_bool(true);
    }
    else
    {
      return val_new_bool(false);
    }
}

/** @brief Built-in delay function to pause script execution for N milliseconds. */
static value_t *built_in_delay(value_t **argv, int argc) {
    if (argc != 1 || argv == NULL || argv[0] == NULL) {
        ti_log("[ERROR] %s: Invalid argument count, expect 1\n", BUILTIN_DELAY);
        ti_fatal();
    }
    switch (argv[0]->type) {
        case VAL_INT:
            if (argv[0]->int_val > 0) {
                vTaskDelay(pdMS_TO_TICKS(argv[0]->int_val));
            }
            break;
        case VAL_FLOAT:
            if (argv[0]->float_val > 0.0f) {
                vTaskDelay(pdMS_TO_TICKS((int)argv[0]->float_val));
            }
            break;
        default:
            ti_log("[ERROR] %s: Unexpected type %d\n", BUILTIN_DELAY, argv[0]->type);
            ti_fatal();
            break;
    }
    return val_new_null();
}

void builtin_system_init(void) {
    static param_t delay_params[] = { { VAL_INT, (char*)"ms" } };
    register_builtin_function(BUILTIN_IS_NONE, VAL_BOOL, NULL, -1, built_in_is_none);
    register_builtin_function(BUILTIN_DELAY, VAL_VOID, delay_params, 1, built_in_delay);
}
