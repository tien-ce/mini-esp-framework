#ifndef CMD_H
#define CMD_H

// ============================================================================
// 1. SYSTEM & CORE COMMANDS
// ============================================================================
#define CMD_SET_LOG_LEVEL   "CMD_SET_LEVEL"
#define CMD_GET_LOG_LEVEL   "CMD_GET_LEVEL"
#define CMD_LIST_LOG_LEVEL  "CMD_LIST_LEVEL"
#define CMD_RESTART         "ESP32_RESTART"
#define CMD_BACKLOG         "BACKLOG"
#define CMD_RULE            "RULE"

// ============================================================================
// 2. WIFI COMMANDS
// ============================================================================
#define CMD_WIFI_SSID       "SSID"
#define CMD_WIFI_PASSWORD   "PASSWORD"
#define CMD_WIFI_CLIENT_ID  "CLIENT_ID"
#define CMD_STATIC_IP       "IPADDRESS"
#define CMD_STATIC_GATEWAY  "GATEWAY"
#define CMD_STATIC_SUBNET   "SUBNETMASK"
#define CMD_STATIC_DNS      "DNSSERVER"
#define CMD_WIFI_STATUS     "WIFI_STATUS"

// ============================================================================
// 3. DRIVER COMMANDS
// ============================================================================
// Relay Driver (xdrv_02_relay.cpp)
#define RELAY1_CMD          "RELAY1"
#define RELAY2_CMD          "RELAY2"
#define RELAY3_CMD          "RELAY3"

// ============================================================================
// 4. SENSOR COMMANDS
// ============================================================================
// Sensor specific command definitions can be added here

#endif // CMD_H
