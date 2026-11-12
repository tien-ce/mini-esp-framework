#ifndef NVS_HANDLE_H
#define NVS_HANDLE_H

/**
 * @file nvs_handle.h
 * @brief Script handle "nvs": one NVS namespace used by a TI script, built on top of Preferences.
 *
 * Layering:
 *
 *     ESP-IDF NVS  ->  Preferences  ->  nvs handle  ->  TI script (storage.NVS())
 *
 * The handle owns the opened namespace: it is opened by the script through the handle and closed when the
 * last value pointing to the handle is destroyed (end of script included).
 *
 * Script view:
 *   methods: open(namespace)         -> opens read/write, the namespace is created when it does not exist
 *            read(key, default)      -> stored value, or default when the key does not exist
 *            write(key, value)       -> bool, value: int, float, bool or string
 *            close()
 *   Calling read / write / close on an nvs that is not open raises an error.
 */

/*---- INCLUDES ----*/
#include <Arduino.h>
#include <Preferences.h>
#include "TienInterpreter.h"

/*---- TYPES ----*/

/**
 * @brief Native object wrapped by an "nvs" handle value. (nvs_handle_t is used by esp_idf so don't wonder why I use value"
 * @note Holds a C++ object (Preferences): allocate with new and free with delete, never calloc / free.
 */
typedef struct {
    Preferences prefs;      /**< Opened namespace, valid while is_open. */
    bool        is_open;    /**< True between open() and close() / destroy. */
} nvs_value_t;

/*---- HANDLE TYPE ----*/

/**
 * @brief Handle type of an nvs value: name "nvs", destructor, methods.
 * @details Wrap a new'ed nvs_value_t with val_new_handle(ptr, &g_nvs_handle_type). The value then owns it:
 *          the destructor closes the namespace if still open and deletes it when the last value is freed.
 */
extern const ti_handle_type_t g_nvs_handle_type;

#endif // NVS_HANDLE_H
