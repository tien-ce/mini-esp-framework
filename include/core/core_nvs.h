#ifndef CORE_NVS_H
#define CORE_NVS_H

/*---- INCLUDES ----*/
#include <Arduino.h>
#include <Preferences.h>
#include <FreeRTOS.h>
#include <semphr.h>

/*---- PUBLIC FUNCTIONS ----*/

/**
 * @brief Initializes the Non-Volatile Storage (NVS) management layer.
 * 
 * Creates the recursive mutex used for thread-safe access to Flash memory
 * and prepares the internal registry for namespace tracking.
 */
void core_nvs_init();

/**
 * @brief Registers an NVS namespace into the system registry.
 * 
 * Validates that the namespace does not exceed the hardware limit (15 characters)
 * and stores it to allow global tracking of active configurations.
 * 
 * @param ns The NVS namespace (maximum 15 characters).
 * @return true if successfully registered, false if invalid or registry full.
 */
bool core_nvs_register_namespace(const String &ns);

/**
 * @brief Checks if an NVS namespace has been registered.
 * 
 * @param ns The NVS namespace to check.
 * @return true if the namespace exists in the registry.
 */
bool core_nvs_is_registered(const String &ns);

/**
 * @brief Checks if a specific key exists within a given namespace.
 * 
 * @param ns The NVS namespace.
 * @param key The configuration key.
 * @return true if the key exists, false otherwise.
 */
bool core_nvs_has_key(const String &ns, const String &key);

/**
 * @brief Clears all keys stored under the specified NVS namespace.
 * 
 * @param ns The NVS namespace to format/clear.
 * @return true if successfully cleared.
 */
bool core_nvs_clear_namespace(const String &ns);

/*---- KEY-VALUE I/O FUNCTIONS ----*/

/**
 * @brief Saves a string value to NVS.
 * 
 * @param ns The NVS namespace.
 * @param key The configuration key.
 * @param value The string payload to save.
 * @return true on success, false on failure.
 */
bool core_nvs_save_string(const String &ns, const String &key, const String &value);

/**
 * @brief Reads a string value from NVS.
 * 
 * @param ns The NVS namespace.
 * @param key The configuration key.
 * @param default_val The fallback value returned if the key does not exist.
 * @return String The retrieved value or default.
 */
String core_nvs_read_string(const String &ns, const String &key, const String &default_val = "");

/**
 * @brief Saves a 32-bit integer value to NVS.
 * 
 * @param ns The NVS namespace.
 * @param key The configuration key.
 * @param value The integer payload to save.
 * @return true on success, false on failure.
 */
bool core_nvs_save_int(const String &ns, const String &key, int32_t value);

/**
 * @brief Reads a 32-bit integer value from NVS.
 * 
 * @param ns The NVS namespace.
 * @param key The configuration key.
 * @param default_val The fallback value returned if the key does not exist.
 * @return int32_t The retrieved value or default.
 */
int32_t core_nvs_read_int(const String &ns, const String &key, int32_t default_val = 0);

/**
 * @brief Saves a boolean value to NVS.
 * 
 * @param ns The NVS namespace.
 * @param key The configuration key.
 * @param value The boolean payload to save.
 * @return true on success, false on failure.
 */
bool core_nvs_save_bool(const String &ns, const String &key, bool value);

/**
 * @brief Reads a boolean value from NVS.
 * 
 * @param ns The NVS namespace.
 * @param key The configuration key.
 * @param default_val The fallback value returned if the key does not exist.
 * @return true or false based on NVS or default.
 */
bool core_nvs_read_bool(const String &ns, const String &key, bool default_val = false);

#endif // CORE_NVS_H
