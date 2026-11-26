/**
 * @file network_module.cpp
 * @brief Script module "network": network resources for scripts (network.HTTP()).
 */

/*---- INCLUDES ----*/
#include <Arduino.h>
#include <new>
#include "TienInterpreter.h"
#include "scripting/handle/network/http_handle.h"
#include "scripting/module/module_manager.h"
#include "core/core_log.h"

/*---- CONFIG ----*/

#define HTTP_DEFAULT_TIMEOUT_MS      5000                  /* Attribute timeout_ms of a new http */
#define HTTP_DEFAULT_CONTENT_TYPE    "application/json"    /* Attribute content_type of a new http */

/*---- STATIC FUNCTION PROTOTYPES ----*/

static value_t *network_http(ti_handle_t handle, value_t **argv, int argc);

/*---- FUNCTIONS ----*/

static const ti_builtin_def_t s_network_funcs[] = {
    { "HTTP", VAL_HANDLE, NULL, 0, network_http },
};

/*---- MODULE ----*/

static const ti_module_t s_network_module = {
    "network",                                                      /* name */
    s_network_funcs,                                                /* funcs */
    (int)(sizeof(s_network_funcs) / sizeof(s_network_funcs[0])),    /* func_count */
    NULL,                                                           /* consts: none */
    0                                                               /* const_count */
};

/*---- STATIC FUNCTIONS ----*/

/**
 * @brief network.HTTP(): create an http handle; the script sends requests with http.get(url), http.post(url, body)
 *        or their _async versions.
 * @param handle Calling runtime handle, used to raise errors.
 * @param argv Unused (no arguments).
 * @param argc Unused.
 * @return Handle value of type "http"; NULL after raising an error.
 */
static value_t *network_http(ti_handle_t handle, value_t **argv, int argc) {
    (void)argv;
    (void)argc;

    // Step 1: the native object; new () runs the HTTPClient and String constructors and zeroes the plain fields
    //         (busy false, on_done and self_value NULL, code 0). nothrow: NULL instead of an exception
    http_handle_t *self = new (std::nothrow) http_handle_t();
    if (self == NULL) {
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "network.HTTP: out of memory");
        return NULL;
    }
    self->timeout_ms   = HTTP_DEFAULT_TIMEOUT_MS;
    self->content_type = HTTP_DEFAULT_CONTENT_TYPE;

    // Step 2: wrap it; on success the value owns self (http_destroy deletes it)
    value_t *v = val_new_handle(self, &g_http_handle_type);
    if (v == NULL) {
        delete self;   // not taken by val_new_handle on failure
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "network.HTTP: out of memory");
        return NULL;
    }
    return v;
}

/*---- PUBLIC FUNCTIONS ----*/

/**
 * @brief Make the module importable by scripts ('import network;').
 */
void network_module_init(void) {
    if (!ti_register_module(&s_network_module)) {
        LOG_ERROR("network module: registration failed");
    }
}
