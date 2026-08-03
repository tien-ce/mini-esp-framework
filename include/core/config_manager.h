#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

/* -------------------------------------------------------------------------- */
/*                     LITTLEFS GENERIC CONFIG MANAGER API                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initializes LittleFS storage and loads registered file mapping list.
 * Must be called at boot before module registrations or config reads/writes.
 * @param None
 * @return None
 */
void loadConfig();

/**
 * @brief Registers a module name and its associated configuration file name.
 * 
 * Checks module_name and file. If file_name is already registered under a
 * different module_name, registration fails and returns false.
 * 
 * @param module_name Unique identifier for module (e.g. "wifi", "web").
 * @param file_name Configuration filename stored in LittleFS (e.g. "wifi_config.txt").
 * @return true on success, false if file_name is already registered to another module.
 */
bool register_config_file(const String &module_name, const String &file_name);

/**
 * @brief Saves configuration content for a registered module to its file in LittleFS.
 * @param module_name Registered module name.
 * @param content Configuration content string to save.
 * @return true on success, false if module is not registered or file error occurs.
 */
bool save_config(const String &module_name, const String &content);

/**
 * @brief Reads configuration content for a registered module from its LittleFS file.
 * @param module_name Registered module name.
 * @return String content of the file, or "" if not found or unreadable.
 */
String read_config(const String &module_name);

/**
 * @brief Checks whether a module is currently registered.
 * @param module_name Unique identifier for module to check.
 * @return true if module is registered, false otherwise.
 */
bool is_module_registered(const String &module_name);

// ==================== RUNTIME SENSOR COUNTER ====================

/**
 * @brief Gets the current runtime sensor count value in a thread-safe manner.
 * @param None
 * @return Current sensor count value.
 */
int getSensorCount();

/**
 * @brief Increments the runtime sensor count by 1 in a thread-safe manner.
 * @param None
 * @return None
 */
void incrementSensorCount();

/**
 * @brief Resets the runtime sensor count to zero in a thread-safe manner.
 * @param None
 * @return None
 */
void resetSensorCount();

#endif // CONFIG_MANAGER_H


