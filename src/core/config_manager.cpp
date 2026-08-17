#include "core/config_manager.h"
#include "core/log_task.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <FreeRTOS.h>
#include <semphr.h>

/* -------------------------------------------------------------------------- */
/*                              STATIC VARIABLES                              */
/* -------------------------------------------------------------------------- */

/* Internal registry using ArduinoJson for O(1) module-to-namespace lookups */
static JsonDocument g_registry;
static SemaphoreHandle_t g_config_mutex = NULL;

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Helper to resolve NVS namespace for a registered module.
 */
static String get_namespace_internal(const String &module_name) {
    if (g_registry.containsKey(module_name)) {
        return g_registry[module_name].as<String>();
    }
    return "";
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

void config_manager_init() {
    if (g_config_mutex == NULL) {
        g_config_mutex = xSemaphoreCreateRecursiveMutex();
    }
    if (config_get_lock()) {
        g_registry.clear();
        config_release_lock();
    }
    LOG_INFO("Config manager (NVS / Preferences) initialized successfully.");
}

bool config_get_lock(TickType_t timeout_ticks) {
    if (g_config_mutex == NULL) {
        g_config_mutex = xSemaphoreCreateRecursiveMutex();
    }
    return (xSemaphoreTakeRecursive(g_config_mutex, timeout_ticks) == pdTRUE);
}

void config_release_lock() {
    if (g_config_mutex != NULL) {
        xSemaphoreGiveRecursive(g_config_mutex);
    }
}

bool register_config_module(const String &module_name, const String &nvs_namespace) {
    if (module_name.length() == 0 || nvs_namespace.length() == 0) {
        LOG_ERROR("Registration failed: module_name or namespace is empty");
        return false;
    }
    if (nvs_namespace.length() > 15) {
        LOG_ERROR("Registration failed: NVS namespace '" + nvs_namespace + "' exceeds 15 characters limit");
        return false;
    }

    bool success = false;
    if (config_get_lock()) {
        g_registry[module_name] = nvs_namespace;
        LOG_INFO("Registered module '" + module_name + "' with NVS namespace '" + nvs_namespace + "'");
        success = true;
        config_release_lock();
    }
    return success;
}

bool is_module_registered(const String &module_name) {
    bool registered = false;
    if (config_get_lock()) {
        registered = g_registry.containsKey(module_name);
        config_release_lock();
    }
    return registered;
}

bool config_has_key(const String &module_name, const String &key) {
    bool exists = false;
    if (config_get_lock()) {
        String ns = get_namespace_internal(module_name);
        if (ns.length() > 0) {
            Preferences prefs;
            if (prefs.begin(ns.c_str(), true)) {
                exists = prefs.isKey(key.c_str());
                prefs.end();
            }
        }
        config_release_lock();
    }
    return exists;
}

bool config_clear_module(const String &module_name) {
    bool ok = false;
    if (config_get_lock()) {
        String ns = get_namespace_internal(module_name);
        if (ns.length() > 0) {
            Preferences prefs;
            if (prefs.begin(ns.c_str(), false)) {
                ok = prefs.clear();
                prefs.end();
            }
        }
        config_release_lock();
    }
    return ok;
}

bool config_save_string(const String &module_name, const String &key, const String &value) {
    bool ok = false;
    if (config_get_lock()) {
        String ns = get_namespace_internal(module_name);
        if (ns.length() > 0) {
            Preferences prefs;
            if (prefs.begin(ns.c_str(), false)) {
                prefs.putString(key.c_str(), value);
                prefs.end();
                ok = true;
            } else {
                LOG_ERROR("Failed to open NVS namespace: " + ns);
            }
        } else {
            LOG_ERROR("Cannot save: Module '" + module_name + "' is not registered.");
        }
        config_release_lock();
    }
    return ok;
}

String config_read_string(const String &module_name, const String &key, const String &default_val) {
    String result = default_val;
    if (config_get_lock()) {
        String ns = get_namespace_internal(module_name);
        if (ns.length() > 0) {
            Preferences prefs;
            if (prefs.begin(ns.c_str(), true)) {
                if (prefs.isKey(key.c_str())) {
                    result = prefs.getString(key.c_str(), default_val);
                }
                prefs.end();
            }
        } else {
            LOG_ERROR("Cannot read: Module '" + module_name + "' is not registered.");
        }
        config_release_lock();
    }
    return result;
}

bool config_save_int(const String &module_name, const String &key, int32_t value) {
    bool ok = false;
    if (config_get_lock()) {
        String ns = get_namespace_internal(module_name);
        if (ns.length() > 0) {
            Preferences prefs;
            if (prefs.begin(ns.c_str(), false)) {
                prefs.putInt(key.c_str(), value);
                prefs.end();
                ok = true;
            } else {
                LOG_ERROR("Failed to open NVS namespace: " + ns);
            }
        } else {
            LOG_ERROR("Cannot save: Module '" + module_name + "' is not registered.");
        }
        config_release_lock();
    }
    return ok;
}

int32_t config_read_int(const String &module_name, const String &key, int32_t default_val) {
    int32_t result = default_val;
    if (config_get_lock()) {
        String ns = get_namespace_internal(module_name);
        if (ns.length() > 0) {
            Preferences prefs;
            if (prefs.begin(ns.c_str(), true)) {
                if (prefs.isKey(key.c_str())) {
                    result = prefs.getInt(key.c_str(), default_val);
                }
                prefs.end();
            }
        } else {
            LOG_ERROR("Cannot read: Module '" + module_name + "' is not registered.");
        }
        config_release_lock();
    }
    return result;
}

bool config_save_bool(const String &module_name, const String &key, bool value) {
    bool ok = false;
    if (config_get_lock()) {
        String ns = get_namespace_internal(module_name);
        if (ns.length() > 0) {
            Preferences prefs;
            if (prefs.begin(ns.c_str(), false)) {
                prefs.putBool(key.c_str(), value);
                prefs.end();
                ok = true;
            } else {
                LOG_ERROR("Failed to open NVS namespace: " + ns);
            }
        } else {
            LOG_ERROR("Cannot save: Module '" + module_name + "' is not registered.");
        }
        config_release_lock();
    }
    return ok;
}

bool config_read_bool(const String &module_name, const String &key, bool default_val) {
    bool result = default_val;
    if (config_get_lock()) {
        String ns = get_namespace_internal(module_name);
        if (ns.length() > 0) {
            Preferences prefs;
            if (prefs.begin(ns.c_str(), true)) {
                if (prefs.isKey(key.c_str())) {
                    result = prefs.getBool(key.c_str(), default_val);
                }
                prefs.end();
            }
        } else {
            LOG_ERROR("Cannot read: Module '" + module_name + "' is not registered.");
        }
        config_release_lock();
    }
    return result;
}

bool config_save_float(const String &module_name, const String &key, float value) {
    bool ok = false;
    if (config_get_lock()) {
        String ns = get_namespace_internal(module_name);
        if (ns.length() > 0) {
            Preferences prefs;
            if (prefs.begin(ns.c_str(), false)) {
                prefs.putFloat(key.c_str(), value);
                prefs.end();
                ok = true;
            } else {
                LOG_ERROR("Failed to open NVS namespace: " + ns);
            }
        } else {
            LOG_ERROR("Cannot save: Module '" + module_name + "' is not registered.");
        }
        config_release_lock();
    }
    return ok;
}

float config_read_float(const String &module_name, const String &key, float default_val) {
    float result = default_val;
    if (config_get_lock()) {
        String ns = get_namespace_internal(module_name);
        if (ns.length() > 0) {
            Preferences prefs;
            if (prefs.begin(ns.c_str(), true)) {
                if (prefs.isKey(key.c_str())) {
                    result = prefs.getFloat(key.c_str(), default_val);
                }
                prefs.end();
            }
        } else {
            LOG_ERROR("Cannot read: Module '" + module_name + "' is not registered.");
        }
        config_release_lock();
    }
    return result;
}
