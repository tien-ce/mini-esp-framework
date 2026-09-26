#include "core/core_nvs.h"
#include "core/core_log.h"

/*---- ENUMS & TYPES ----*/
/** 
 * @brief Linked list node for storing NVS namespaces dynamically.
 * 
 * Using a linked list removes the hard limit on the number of modules
 * and only consumes RAM for namespaces that actually exist.
 */
struct NvsNode {
    char ns[16];       // The namespace string (max 15 chars + null terminator)
    NvsNode* next;     // Pointer to the next node in the list
};

/*---- STATIC VARIABLES ----*/
/** @brief Recursive mutex ensuring thread-safe access to NVS read/write operations. */
static SemaphoreHandle_t nvsMutex = NULL;

/** @brief Pointer to the head of the NVS namespace linked list. */
static NvsNode* nvsListHead = NULL;

/*---- STATIC DECLARATIONS ----*/
// (None required for this file)

/*---- PUBLIC FUNCTIONS ----*/

/**
 * @brief Initializes the Non-Volatile Storage (NVS) management layer.
 */
void core_nvs_init() {
    if (nvsMutex == NULL) {
        nvsMutex = xSemaphoreCreateRecursiveMutex();
    }
    
    // Linked list head is naturally NULL on boot, no need to clear an array
    LOG_INFO("Core NVS initialized successfully.");
}

/**
 * @brief Acquires the recursive mutex for NVS operations.
 */
static bool core_nvs_get_lock(TickType_t timeout_ticks = portMAX_DELAY) {
    return (xSemaphoreTakeRecursive(nvsMutex, timeout_ticks) == pdTRUE);
}

/**
 * @brief Releases the recursive mutex for NVS operations.
 */
static void core_nvs_release_lock() {
    xSemaphoreGiveRecursive(nvsMutex);
}

/**
 * @brief Registers an NVS namespace into the system registry.
 */
bool core_nvs_register_namespace(const String &ns) {
    // Hardware limit constraint check (ESP-IDF limits NVS namespace to 15 chars)
    if (ns.length() == 0 || ns.length() > 15) {
        LOG_ERROR("NVS registration failed: Namespace must be between 1 and 15 characters");
        return false;
    }

    bool success = false;
    if (core_nvs_get_lock()) {
        NvsNode* current = nvsListHead;
        NvsNode* previous = NULL;

        // 1. Traverse the linked list to check if namespace already exists
        while (current != NULL) {
            if (strcmp(current->ns, ns.c_str()) == 0) {
                // If it already exists, return false immediately as per requirement
                LOG_WARNING("NVS namespace '" + ns + "' is already registered.");
                core_nvs_release_lock();
                return false;
            }
            // Keep track of the last node (previous) to easily append at the tail later
            previous = current;
            current = current->next;
        }
        
        // 2. Namespace not found. Allocate memory for a new node using C-style malloc.
        NvsNode* newNode = (NvsNode*)malloc(sizeof(NvsNode));
        if (newNode == NULL) {
            LOG_ERROR("NVS registration failed: Out of memory (malloc failed)");
            core_nvs_release_lock();
            return false;
        }

        // Safely copy string into the char array (max 15 chars) and null-terminate
        strncpy(newNode->ns, ns.c_str(), 15);
        newNode->ns[15] = '\0'; 
        newNode->next = NULL; // As this will be the tail, next is NULL

        // 3. Insert the new node into the linked list
        if (nvsListHead == NULL) {
            // Case A: The list is completely empty, so this node becomes the head
            nvsListHead = newNode;
        } else {
            // Case B: Append to the tail. 'previous' holds the last valid node from the while loop above
            previous->next = newNode;
        }
        
        LOG_INFO("Registered NVS namespace: '" + ns + "'");
        success = true;
        
        core_nvs_release_lock();
    }
    return success;
}

/**
 * @brief Checks if an NVS namespace has been registered.
 */
bool core_nvs_is_registered(const String &ns) {
    bool registered = false;
    if (core_nvs_get_lock()) {
        // Traverse the linked list from the head
        NvsNode* current = nvsListHead;
        while (current != NULL) {
            if (strcmp(current->ns, ns.c_str()) == 0) {
                registered = true;
                break; // Found it, stop searching
            }
            current = current->next; // Move to the next node
        }
        core_nvs_release_lock();
    }
    return registered;
}

/**
 * @brief Checks if a specific key exists within a given namespace.
 */
bool core_nvs_has_key(const String &ns, const String &key) {
    bool exists = false;
    if (core_nvs_get_lock()) {
        Preferences prefs;
        if (prefs.begin(ns.c_str(), true)) {
            exists = prefs.isKey(key.c_str());
            prefs.end();
        }
        core_nvs_release_lock();
    }
    return exists;
}

/**
 * @brief Clears all keys stored under the specified NVS namespace.
 */
bool core_nvs_clear_namespace(const String &ns) {
    bool cleared = false;
    if (core_nvs_get_lock()) {
        Preferences prefs;
        if (prefs.begin(ns.c_str(), false)) { // false = RW mode
            cleared = prefs.clear();
            prefs.end();
            if (cleared) LOG_INFO("Cleared NVS namespace: " + ns);
        }
        core_nvs_release_lock();
    }
    return cleared;
}

/*---- KEY-VALUE I/O FUNCTIONS ----*/

/**
 * @brief Saves a string value to NVS.
 */
bool core_nvs_save_string(const String &ns, const String &key, const String &value) {
    bool success = false;
    if (core_nvs_get_lock()) {
        Preferences prefs;
        if (prefs.begin(ns.c_str(), false)) {
            size_t written = prefs.putString(key.c_str(), value);
            success = (written > 0);
            prefs.end();
        }
        core_nvs_release_lock();
    }
    return success;
}

/**
 * @brief Reads a string value from NVS.
 */
String core_nvs_read_string(const String &ns, const String &key, const String &default_val) {
    String value = default_val;
    if (core_nvs_get_lock()) {
        Preferences prefs;
        if (prefs.begin(ns.c_str(), true)) {
            value = prefs.getString(key.c_str(), default_val);
            prefs.end();
        }
        core_nvs_release_lock();
    }
    return value;
}

/**
 * @brief Saves a 32-bit integer value to NVS.
 */
bool core_nvs_save_int(const String &ns, const String &key, int32_t value) {
    bool success = false;
    if (core_nvs_get_lock()) {
        Preferences prefs;
        if (prefs.begin(ns.c_str(), false)) {
            size_t written = prefs.putInt(key.c_str(), value);
            success = (written > 0);
            prefs.end();
        }
        core_nvs_release_lock();
    }
    return success;
}

/**
 * @brief Reads a 32-bit integer value from NVS.
 */
int32_t core_nvs_read_int(const String &ns, const String &key, int32_t default_val) {
    int32_t value = default_val;
    if (core_nvs_get_lock()) {
        Preferences prefs;
        if (prefs.begin(ns.c_str(), true)) {
            value = prefs.getInt(key.c_str(), default_val);
            prefs.end();
        }
        core_nvs_release_lock();
    }
    return value;
}

/**
 * @brief Saves a boolean value to NVS.
 */
bool core_nvs_save_bool(const String &ns, const String &key, bool value) {
    bool success = false;
    if (core_nvs_get_lock()) {
        Preferences prefs;
        if (prefs.begin(ns.c_str(), false)) {
            size_t written = prefs.putBool(key.c_str(), value);
            success = (written > 0);
            prefs.end();
        }
        core_nvs_release_lock();
    }
    return success;
}

/**
 * @brief Reads a boolean value from NVS.
 */
bool core_nvs_read_bool(const String &ns, const String &key, bool default_val) {
    bool value = default_val;
    if (core_nvs_get_lock()) {
        Preferences prefs;
        if (prefs.begin(ns.c_str(), true)) {
            value = prefs.getBool(key.c_str(), default_val);
            prefs.end();
        }
        core_nvs_release_lock();
    }
    return value;
}
