/**
 * @file storage_module.cpp
 * @brief Script module "storage": storage resources for scripts (storage.NVS(), later storage.FILE()).
 */

/*---- INCLUDES ----*/
#include <Arduino.h>
#include <new>
#include "TienInterpreter.h"
#include "scripting/handle/storage/nvs_handle.h"
#include "scripting/module/module_manager.h"
#include "core/core_log.h"

/*---- STATIC FUNCTION PROTOTYPES ----*/

static value_t *storage_nvs(ti_handle_t handle, value_t **argv, int argc);

/*---- FUNCTIONS ----*/

static const ti_builtin_def_t s_storage_funcs[] = {
    { "NVS", VAL_HANDLE, NULL, 0, storage_nvs },
};

/*---- MODULE ----*/

static const ti_module_t s_storage_module = {
    "storage",                                                      /* name */
    s_storage_funcs,                                                /* funcs */
    (int)(sizeof(s_storage_funcs) / sizeof(s_storage_funcs[0])),    /* func_count */
    NULL,                                                           /* consts: none */
    0                                                               /* const_count */
};

/*---- STATIC FUNCTIONS ----*/

/**
 * @brief storage.NVS(): create a closed nvs handle; the script opens a namespace with nvs.open(namespace).
 * @param handle Calling runtime handle, used to raise errors.
 * @param argv Unused (no arguments).
 * @param argc Unused.
 * @return Handle value of type "nvs"; NULL after raising an error.
 */
static value_t *storage_nvs(ti_handle_t handle, value_t **argv, int argc) {
    (void)argv;
    (void)argc;

    // Step 1: the native object; new runs the Preferences constructor (nothrow: NULL instead of an exception)
    nvs_value_t *self = new (std::nothrow) nvs_value_t();
    if (self == NULL) {
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "storage.NVS: out of memory");
        return NULL;
    }

    // Step 2: wrap it; on success the value owns self (nvs_destroy deletes it)
    value_t *v = val_new_handle(self, &g_nvs_handle_type);
    if (v == NULL) {
        delete self;   // not taken by val_new_handle on failure
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "storage.NVS: out of memory");
        return NULL;
    }
    return v;
}

/*---- PUBLIC FUNCTIONS ----*/

/**
 * @brief Make the module importable by scripts ('import storage;').
 */
void storage_module_init(void) {
    if (!ti_register_module(&s_storage_module)) {
        LOG_ERROR("storage module: registration failed");
    }
}
