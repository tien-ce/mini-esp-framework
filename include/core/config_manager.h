#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <FreeRTOS.h>
#include <semphr.h>

/* -------------------------------------------------------------------------- */
/*                        NVS GENERIC CONFIG MANAGER API                      */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initializes NVS config manager and prepares the registry.
 */
void config_manager_init();

/**
 * @brief Acquires the recursive configuration mutex lock.
 * Allows modules to read/write multiple configuration parameters atomically on boot.
 * 
 * @param timeout_ticks Maximum ticks to wait for mutex (defaults to portMAX_DELAY).
 * @return true if lock acquired successfully, false otherwise.
 */
bool config_get_lock(TickType_t timeout_ticks = portMAX_DELAY);

/**
 * @brief Releases the recursive configuration mutex lock.
 */
void config_release_lock();

/**
 * @brief Registers a module name and its associated NVS namespace.
 * 
 * @param module_name Unique identifier for the module (e.g., "wifi", "web").
 * @param nvs_namespace NVS namespace name (max 15 characters, ESP-IDF constraint).
 * @return true if registration succeeded, false if invalid or namespace > 15 chars.
 */
bool register_config_module(const String &module_name, const String &nvs_namespace);

/**
 * @brief Checks if a module is currently registered in the manager.
 */
bool is_module_registered(const String &module_name);

/**
 * @brief Checks if a specific key exists in the module's NVS storage.
 */
bool config_has_key(const String &module_name, const String &key);

/**
 * @brief Clears all keys stored under the module's NVS namespace.
 */
bool config_clear_module(const String &module_name);

/* -------------------------------------------------------------------------- */
/*                       KEY-VALUE GET / SET API (TYPED)                      */
/* -------------------------------------------------------------------------- */

/**
 * @brief Saves a string value to NVS under the specified module and key.
 */
bool config_save_string(const String &module_name, const String &key, const String &value);

/**
 * @brief Reads a string value from NVS under the specified module and key.
 */
String config_read_string(const String &module_name, const String &key, const String &default_val = "");

/**
 * @brief Saves an integer value to NVS under the specified module and key.
 */
bool config_save_int(const String &module_name, const String &key, int32_t value);

/**
 * @brief Reads an integer value from NVS under the specified module and key.
 */
int32_t config_read_int(const String &module_name, const String &key, int32_t default_val = 0);

/**
 * @brief Saves a boolean value to NVS under the specified module and key.
 */
bool config_save_bool(const String &module_name, const String &key, bool value);

/**
 * @brief Reads a boolean value from NVS under the specified module and key.
 */
bool config_read_bool(const String &module_name, const String &key, bool default_val = false);

/**
 * @brief Saves a float value to NVS under the specified module and key.
 */
bool config_save_float(const String &module_name, const String &key, float value);

/**
 * @brief Reads a float value from NVS under the specified module and key.
 */
float config_read_float(const String &module_name, const String &key, float default_val = 0.0f);

/* -------------------------------------------------------------------------- */
/*                      BACKWARD COMPATIBILITY ALIASES                        */
/* -------------------------------------------------------------------------- */

/** @brief Legacy alias for register_config_module. */
inline bool register_config_file(const String &module_name, const String &file_name) {
    return register_config_module(module_name, module_name);
}

#endif // CONFIG_MANAGER_H
