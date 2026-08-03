#ifndef CORE_ENGINE_H
#define CORE_ENGINE_H

#include <Arduino.h>
#include "core/config_manager.h"
#include "core/log_task.h"
#include "core/wifi_task.h"
#include "core/web_server_task.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// ============================================================================
// 1. DOMAIN-SPECIFIC STATE ENUMERATIONS
// ============================================================================

typedef enum {
    SYSTEM_EVENT = 0,
    NETWORK_EVENT,
    WEB_EVENT,
    MQTT_EVENT
} Event_t;

/**
 * @brief System Operational Mode
 * Represents the high-level operating context of the device.
 */
typedef enum {
    MODE_BOOT = 0,          /**< System powering on; queues and heap allocating */
    MODE_SETUP,             /**< Reading NVS configs & starting initial core tasks */
    MODE_NORMAL,            /**< Standard production execution mode, after as least serial start */
    MODE_PROVISIONING,      /**< AP/Setup mode waiting for credentials */
    MODE_OTA_UPDATE,        /**< Firmware update in progress */
    MODE_SAFE_MODE          /**< Hardware failure or critical config missing */
} SystemMode_t;

/**
 * @brief Layer-2 / Layer-3 Network Connectivity State
 * Managed exclusively by `wifi_task`.
 */
typedef enum {
    NET_STATE_DISCONNECTED = 0, /**< Wi-Fi radio off or disconnected from AP */
    NET_STATE_CONNECTING,       /**< Actively negotiating link or waiting for DHCP IP */
    NET_STATE_WIFI_STA,         /**< Connected as Station with valid IP assigned */
    NET_STATE_WIFI_AP           /**< Operating as local Access Point (Fallback / Portal) */
} NetworkState_t;

/**
 * @brief Web Server & WebSocket Service State
 * Managed exclusively by `web_server`.
 */
typedef enum {
    WEB_STATE_STOPPED = 0,      /**< HTTP listening sockets bound down / inactive */
    WEB_STATE_LISTENING,        /**< HTTP server running on port 80/8088 waiting for requests */
    WEB_STATE_CLIENT_CONNECTED  /**< HTTP server active with 1 or more active WebSocket clients */
} WebServerState_t;

/**
 * @brief Layer-7 MQTT Broker Service State
 * Managed exclusively by `mqtt_task`.
 */
typedef enum {
    MQTT_STATE_DISCONNECTED = 0,/**< Broker socket closed or inactive */
    MQTT_STATE_CONNECTING,      /**< TCP socket open; negotiating MQTT CONNECT/CONNACK */
    MQTT_STATE_CONNECTED,       /**< Authenticated with broker; ready to pub/sub telemetry */
    MQTT_STATE_ERROR            /**< Authentication failed or invalid broker endpoint */
} MqttState_t;

// ============================================================================
// 2. COMPOSITE SYSTEM STATE MATRIX
// ============================================================================

/**
 * @brief Complete System State Snapshot Structure
 * Encapsulates the independent status of all core subsystems.
 */
typedef struct {
    SystemMode_t     mode;        /**< Global system mode context */
    NetworkState_t   network;     /**< L2/L3 Network status */
    WebServerState_t web;         /**< WebServer/WebSocket service status */
    MqttState_t      mqtt;        /**< MQTT client status */
    bool             storage_ok;  /**< NVS / Flash Storage integrity flag */
    uint32_t         last_update; /**< System tick timestamp of last state change */
} CoreSystemStateMatrix_t;

// ============================================================================
// 3. THREAD-SAFE STATE MATRIX API INTERFACE
// ============================================================================

/**
 * @brief Thread-safe getter to read the current system state matrix snapshot.
 * @param p_out_state Pointer to structure where state snapshot will be copied.
 * @return None
 */
void CoreState_Get(CoreSystemStateMatrix_t* p_out_state);

/**
 * @brief Thread-safe getter for current system mode.
 * @param None
 * @return Current SystemMode_t enum value.
 */
SystemMode_t CoreState_GetMode();

/**
 * @brief Thread-safe getter for current network state.
 * @param None
 * @return Current NetworkState_t enum value.
 */
NetworkState_t CoreState_GetNetwork();

/**
 * @brief Thread-safe getter for current web server state.
 * @param None
 * @return Current WebServerState_t enum value.
 */
WebServerState_t CoreState_GetWebServer();

/**
 * @brief Thread-safe getter for current MQTT client state.
 * @param None
 * @return Current MqttState_t enum value.
 */
MqttState_t CoreState_GetMqtt();

/**
 * @brief Thread-safe getter for flash storage integrity status.
 * @param None
 * @return true if storage is readable/writable, false on failure.
 */
bool CoreState_GetStorageStatus();

/**
 * @brief Thread-safe update for System Mode context.
 * @param mode New SystemMode_t enum value.
 * @return None
 */
void CoreState_SetMode(SystemMode_t mode);

/**
 * @brief Thread-safe update for Network State.
 * @param state New NetworkState_t enum value.
 * @return None
 */
void CoreState_SetNetwork(NetworkState_t state);

/**
 * @brief Thread-safe update for WebServer State.
 * @param state New WebServerState_t enum value.
 * @return None
 */
void CoreState_SetWebServer(WebServerState_t state);

/**
 * @brief Thread-safe update for MQTT Client State.
 * @param state New MqttState_t enum value.
 * @return None
 */
void CoreState_SetMqtt(MqttState_t state);

/**
 * @brief Thread-safe update for Flash Storage health flag.
 * @param is_ok true if storage is readable/writable, false on failure.
 * @return None
 */
void CoreState_SetStorageStatus(bool is_ok);

/**
 * @brief Blocks calling task until a specific subsystem reaches the expected state.
 * @param type Domain category (SYSTEM_EVENT, NETWORK_EVENT, WEB_EVENT, MQTT_EVENT).
 * @param expected_state Target state casted as uint8_t (e.g., NET_STATE_WIFI_STA).
 * @param timeout_ticks Max FreeRTOS ticks to block (e.g., portMAX_DELAY).
 * @return true if state was reached, false on timeout or invalid parameter.
 */
bool waiting_on_event(Event_t type, uint8_t expected_state, TickType_t timeout_ticks);

/**
 * @brief Bootstrapping entry point for the Core Engine framework.
 * 
 * Loads LittleFS configuration, initializes log queues & mutexes, connects WiFi,
 * configures WebServer REST & WS endpoints, and spawns core system FreeRTOS tasks.
 * @param None
 * @return None
 */
void CoreEngine_Start();


#endif // CORE_ENGINE_H
