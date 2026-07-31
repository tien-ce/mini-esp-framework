# Future Architecture Spec: Relational Command Database System

## Overview
This document specifies the design for a **Relational Dual-Hashmap Command Registry System** to be implemented in `log_task`.

Instead of iterating through lists or performing full table scans, this architecture adapts **database indexing concepts** to microcontrollers (ESP32 / C++). It provides **O(1) constant-time performance** for both **command execution** and **module help listing**.

---

## Relational Dual-Index Architecture

```
   [ Input String: "LOG" or "SET_LOG_LEVEL" ]
                       |
         +-------------+-------------+
         |                           |
         v                           v
  TABLE 1: moduleMap          TABLE 2: commandMap
 (Module Name -> List of Cmds) (Cmd Name -> Handler & Module)
  "LOG" -> ["SET_LOG_LEVEL",   "SET_LOG_LEVEL" -> { setLogLevel, "LOG" }
            "GET_LOG_LEVEL"]   "GET_LOG_LEVEL" -> { getLogLevel, "LOG" }
```

---

## API Prototype

```cpp
/**
 * @brief Registers a command string under a specific module owner.
 * @param moduleName The owning module name (e.g., "LOG", "SENSOR", "SYSTEM").
 * @param cmdName The globally unique command name (e.g., "SET_LOG_LEVEL").
 * @param handler Function pointer callback to execute when the command is received.
 * @return true if successfully registered, false if command already exists.
 */
bool register_cmd(const String &moduleName, const String &cmdName, CommandHandlerFunc handler);
```

---

## System Constraints & Rules

1. **Global Command Uniqueness**: Every `cmdName` must be unique across all modules. Registering a duplicate `cmdName` will fail and log a warning.
2. **Module Grouping & Auto-Help**:
   - If an incoming command input matches a `moduleName` (e.g., `"LOG"`), `log_task` looks up `moduleMap` in O(1) time and prints all commands registered under that module.
3. **O(1) Instant Command Execution**:
   - If input matches a `cmdName`, `log_task` looks up `commandMap` in O(1) time and executes its handler.

---

## Data Structures

```cpp
#include <Arduino.h>
#include <unordered_map>
#include <vector>

// Custom String Hash Struct for std::unordered_map
struct StringHash {
    std::size_t operator()(const String& s) const {
        return std::hash<std::string>{}(s.c_str());
    }
};

// Table 1: Command Record (Cmd Name -> Handler & Module)
struct CommandRecord {
    String moduleName;
    CommandHandlerFunc handler;
};

// Table 1 Index: Primary Command Lookup
static std::unordered_map<String, CommandRecord, StringHash> commandMap;

// Table 2 Index: Module Grouping Lookup
static std::unordered_map<String, std::vector<String>, StringHash> moduleMap;
```

---

## Implementation Details

### 1. Command Registration (`register_cmd`)

```cpp
bool register_cmd(const String &moduleName, const String &cmdName, CommandHandlerFunc handler) {
    String cleanModule = moduleName;
    String cleanCmd = cmdName;
    cleanModule.trim(); cleanModule.toUpperCase();
    cleanCmd.trim();    cleanCmd.toUpperCase();

    // 1. Check Global Uniqueness
    if (commandMap.find(cleanCmd) != commandMap.end()) {
        LOG_WARN("Can't register '" + cleanCmd + "': command already exists!");
        return false;
    }

    // 2. Insert into Primary Command Index (Table 1)
    commandMap[cleanCmd] = { cleanModule, handler };

    // 3. Insert into Module Index (Table 2)
    moduleMap[cleanModule].push_back(cleanCmd);

    LOG_INFO("Registered [" + cleanModule + "] -> " + cleanCmd);
    return true;
}
```

---

### 2. Command Query & Execution (`execute_cmd`)

```cpp
static void execute_cmd(const String &raw_cmd) {
    String cleanCmd = raw_cmd;
    cleanCmd.trim();
    cleanCmd.toUpperCase();

    String name = "";
    String arg = "";

    // Parse "NAME: ARG"
    int colonIdx = cleanCmd.indexOf(':');
    if (colonIdx != -1) {
        name = cleanCmd.substring(0, colonIdx);
        arg = cleanCmd.substring(colonIdx + 1);
    } else {
        name = cleanCmd;
        arg = "";
    }
    name.trim(); arg.trim();

    // STEP A: Check if 'name' is a Module Name (Auto-Help)
    auto modIt = moduleMap.find(name);
    if (modIt != moduleMap.end()) {
        LOG_INFO("=== Registered Commands for Module [" + name + "] ===");
        for (const String &cmd : modIt->second) {
            LOG_INFO("  - " + cmd);
        }
        return;
    }

    // STEP B: Check if 'name' is a Command Name
    auto cmdIt = commandMap.find(name);
    if (cmdIt != commandMap.end()) {
        LOG_DEBUG("Executing handler for: " + name);
        cmdIt->second.handler(arg);
        return;
    }

    // STEP C: Not Found
    LOG_WARN("Unknown command or module: " + name);
}
```

---

## Module Self-Registration Pattern

When creating new modules in the future, each module registers its commands during initialization:

```cpp
// Inside log_task.cpp initLogTask():
register_cmd("LOG", "SET_LOG_LEVEL",  setLogLevel);
register_cmd("LOG", "GET_LOG_LEVEL",  getLogLevel);
register_cmd("LOG", "LIST_LOG_LEVEL", listLogLevel);

// Inside future sensor_task.cpp:
register_cmd("SENSOR", "RESET_COUNT", resetSensorCountHandler);

// Inside future system_task.cpp:
register_cmd("SYSTEM", "ESP_RESTART", espRestartHandler);
```
