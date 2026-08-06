#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

/* -------------------------------------------------------------------------- */
/*                     LITTLEFS GENERIC CONFIG MANAGER API                    */
/* -------------------------------------------------------------------------- */

/** @brief Initializes config manager and loads registry list. */
void config_manager_init();

/** @brief Registers module name and associated config file. */
bool register_config_file(const String &module_name, const String &file_name);

/** @brief Saves configuration content for a registered module. */
bool save_config(const String &module_name, const String &content);

/** @brief Reads configuration content for a registered module. */
String read_config(const String &module_name);

/** @brief Checks if a module is currently registered. */
bool is_module_registered(const String &module_name);

#endif // CONFIG_MANAGER_H


