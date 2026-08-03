#include "core/config_manager.h"
#include "core/log_task.h"
#include <FreeRTOS.h>
#include <semphr.h>
#include <vector>

#define REGISTRY_FILE_PATH "/sys_config_registry.txt"

struct ModuleRegistryEntry {
    String module_name;
    String file_name;
};

static std::vector<ModuleRegistryEntry> g_registry_list;

// Runtime state variables & mutexes
static int cnt = 0;
static SemaphoreHandle_t countMutex = NULL;
static SemaphoreHandle_t configMutex = NULL;

/**
 * @brief Initializes the FreeRTOS mutexes for config and counter protection.
 * @param None
 * @return None
 */
static void initMutexes() {
    if (countMutex == NULL) {
        countMutex = xSemaphoreCreateMutex();
    }
    if (configMutex == NULL) {
        configMutex = xSemaphoreCreateMutex();
    }
}

/**
 * @brief Writes the internal module registry list to LittleFS system registry file.
 * @param None
 * @return None
 */
static void save_registry_list_internal() {
    File f = LittleFS.open(REGISTRY_FILE_PATH, "w");
    if (!f) {
        return;
    }
    for (const auto &entry : g_registry_list) {
        f.println(entry.module_name + "=" + entry.file_name);
    }
    f.close();
}

/**
 * @brief Reads the module registry list from LittleFS system registry file into RAM.
 * @param None
 * @return None
 */
static void load_registry_list_internal() {
    g_registry_list.clear();
    if (!LittleFS.exists(REGISTRY_FILE_PATH)) {
        return;
    }
    File f = LittleFS.open(REGISTRY_FILE_PATH, "r");
    if (!f) {
        return;
    }
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        int eqIdx = line.indexOf('=');
        if (eqIdx > 0) {
            ModuleRegistryEntry entry;
            entry.module_name = line.substring(0, eqIdx);
            entry.file_name = line.substring(eqIdx + 1);
            entry.module_name.trim();
            entry.file_name.trim();
            g_registry_list.push_back(entry);
        }
    }
    f.close();
}


void loadConfig() {
    initMutexes();
    if (configMutex != NULL && xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        if (!LittleFS.begin(true)) {
            LOG_ERROR("LittleFS Mount Failed!");
        } else {
            LOG_INFO("LittleFS Mounted Successfully.");
            load_registry_list_internal();
        }
        xSemaphoreGive(configMutex);
    }
}

bool register_config_file(const String &module_name, const String &file_name) {
    bool result = false;
    if (configMutex != NULL && xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        String trimmedModule = module_name;
        String trimmedFile = file_name;
        trimmedModule.trim();
        trimmedFile.trim();

        // 1. Check if file is already registered with another module
        bool conflict = false;
        bool existingFound = false;

        for (auto &entry : g_registry_list) {
            if (entry.file_name == trimmedFile) {
                if (entry.module_name != trimmedModule) {
                    conflict = true;
                    break;
                } else {
                    existingFound = true;
                }
            } else if (entry.module_name == trimmedModule) {
                // Same module name, update file name
                entry.file_name = trimmedFile;
                save_registry_list_internal();
                existingFound = true;
            }
        }

        if (conflict) {
            LOG_ERROR("Registration failed: File '" + trimmedFile + "' is already registered to another module!");
            result = false;
        } else if (existingFound) {
            result = true;
        } else {
            // New module registration
            ModuleRegistryEntry newEntry;
            newEntry.module_name = trimmedModule;
            newEntry.file_name = trimmedFile;
            g_registry_list.push_back(newEntry);
            save_registry_list_internal();
            LOG_INFO("Registered module '" + trimmedModule + "' with file '" + trimmedFile + "'");
            result = true;
        }

        xSemaphoreGive(configMutex);
    }
    return result;
}

bool save_config(const String &module_name, const String &content) {
    bool success = false;
    if (configMutex != NULL && xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        String targetFile = "";
        for (const auto &entry : g_registry_list) {
            if (entry.module_name == module_name) {
                targetFile = entry.file_name;
                break;
            }
        }

        if (targetFile.length() > 0) {
            String path = targetFile.startsWith("/") ? targetFile : "/" + targetFile;
            File f = LittleFS.open(path, "w");
            if (f) {
                f.print(content);
                f.close();
                success = true;
            } else {
                LOG_ERROR("Failed to open file for writing: " + path);
            }
        } else {
            LOG_ERROR("Cannot save config: Module '" + module_name + "' is not registered.");
        }
        xSemaphoreGive(configMutex);
    }
    return success;
}

String read_config(const String &module_name) {
    String content = "";
    if (configMutex != NULL && xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        String targetFile = "";
        for (const auto &entry : g_registry_list) {
            if (entry.module_name == module_name) {
                targetFile = entry.file_name;
                break;
            }
        }

        if (targetFile.length() > 0) {
            String path = targetFile.startsWith("/") ? targetFile : "/" + targetFile;
            if (LittleFS.exists(path)) {
                File f = LittleFS.open(path, "r");
                if (f) {
                    content = f.readString();
                    f.close();
                } else {
                    LOG_ERROR("Failed to open file for reading: " + path);
                }
            }
        } else {
            LOG_ERROR("Cannot read config: Module '" + module_name + "' is not registered.");
        }
        xSemaphoreGive(configMutex);
    }
    return content;
}

bool is_module_registered(const String &module_name) {
    bool registered = false;
    if (configMutex != NULL && xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        for (const auto &entry : g_registry_list) {
            if (entry.module_name == module_name) {
                registered = true;
                break;
            }
        }
        xSemaphoreGive(configMutex);
    }
    return registered;
}
