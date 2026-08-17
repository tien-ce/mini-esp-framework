#ifndef CMD_H
#define CMD_H

// ============================================================================
// 1. SYSTEM & CORE COMMANDS
// ============================================================================
#define CMD_SET_LOG_LEVEL   "CMD_SET_LEVEL"
#define CMD_GET_LOG_LEVEL   "CMD_GET_LEVEL"
#define CMD_LIST_LOG_LEVEL  "CMD_LIST_LEVEL"
#define CMD_RESTART         "ESP32_RESTART"
#define CMD_RULE            "RULE"

// ============================================================================
// 2. DRIVER COMMANDS
// ============================================================================
// Relay Driver (xdrv_02_relay.cpp)
#define RELAY1_CMD          "RELAY1"
#define RELAY2_CMD          "RELAY2"
#define RELAY3_CMD          "RELAY3"

// ============================================================================
// 3. SENSOR COMMANDS
// ============================================================================
// Sensor specific command definitions can be added here

#endif // CMD_H
