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
    MQTT_EVENT,
	MAX_EVENT
} Event_t;

/**
 * @brief System Operational Mode
 * Represents the high-level operating context of the device.
 */
typedef enum {
    SYS_BOOT = 0,         /**< System powering on; queues and heap allocating */
    SYS_SETUP,            /**< Reading NVS configs & starting initial core tasks */
    SYS_NORMAL,           /**< Standard production execution mode, after at least serial start */
    SYS_PROVISIONING,     /**< AP/Setup mode waiting for credentials */
    SYS_OTA_UPDATE,       /**< Firmware update in progress */
    SYS_SAFE_MODE,        /**< Hardware failure or critical config missing */
    SYS_MAX_STATE,

    /* --- Deprecated aliases for v1.0.0 compatibility --- */
    MODE_BOOT         __attribute__((deprecated("Use SYS_BOOT instead")))         = SYS_BOOT,
    MODE_SETUP        __attribute__((deprecated("Use SYS_SETUP instead")))        = SYS_SETUP,
    MODE_NORMAL       __attribute__((deprecated("Use SYS_NORMAL instead")))       = SYS_NORMAL,
    MODE_PROVISIONING __attribute__((deprecated("Use SYS_PROVISIONING instead"))) = SYS_PROVISIONING,
    MODE_OTA_UPDATE   __attribute__((deprecated("Use SYS_OTA_UPDATE instead")))   = SYS_OTA_UPDATE,
    MODE_SAFE_MODE    __attribute__((deprecated("Use SYS_SAFE_MODE instead")))    = SYS_SAFE_MODE
} SystemState_t;

/**
 * @brief Layer-2 / Layer-3 Network Connectivity State
 * Managed exclusively by `wifi_task`.
 */
typedef enum {
    NET_STATE_DISCONNECTED = 0, /**< Wi-Fi radio off or disconnected from AP */
    NET_STATE_CONNECTING,       /**< Actively negotiating link or waiting for DHCP IP */
    NET_STATE_WIFI_STA,         /**< Connected as Station with valid IP assigned */
    NET_STATE_WIFI_AP,			/**< Operating as local Access Point (Fallback / Portal) */
	NET_MAX_STATE
} NetworkState_t;
/**
 * @brief Web Server & WebSocket Service State
 * Managed exclusively by `web_server`.
 */
typedef enum {
    WEB_STATE_STOPPED = 0,      /**< HTTP listening sockets bound down / inactive */
    WEB_STATE_LISTENING,        /**< HTTP server running on port 80/8088 waiting for requests */
    WEB_STATE_CLIENT_CONNECTED, /**< HTTP server active with 1 or more active WebSocket clients */
	WEB_MAX_STATE
} WebServerState_t;
/**
 * @brief Layer-7 MQTT Broker Service State
 * Managed exclusively by `mqtt_task`.
 */
typedef enum {
    MQTT_STATE_DISCONNECTED = 0,/**< Broker socket closed or inactive */
    MQTT_STATE_CONNECTING,      /**< TCP socket open; negotiating MQTT CONNECT/CONNACK */
    MQTT_STATE_CONNECTED,       /**< Authenticated with broker; ready to pub/sub telemetry */
    MQTT_STATE_ERROR,            /**< Authentication failed or invalid broker endpoint */
	MQTT_MAX_STATE
} MqttState_t;

// ============================================================================
// 2. COMPOSITE SYSTEM STATE MATRIX
// ============================================================================

/**
 * @brief Complete System State Snapshot Structure
 * Encapsulates the independent status of all core subsystems.
 */
typedef struct {
    SystemState_t     mode;        /**< Global system mode context */
    NetworkState_t   network;     /**< L2/L3 Network status */
    WebServerState_t web;         /**< WebServer/WebSocket service status */
    MqttState_t      mqtt;        /**< MQTT client status */
    bool             storage_ok;  /**< NVS / Flash Storage integrity flag */
    uint32_t         last_update; /**< System tick timestamp of last state change */
} CoreSystemStateMatrix_t;

// ============================================================================
// 3. THREAD-SAFE STATE MATRIX API INTERFACE
// ============================================================================

/** @brief Reads current system state matrix snapshot. */
void CoreState_Get(CoreSystemStateMatrix_t* p_out_state);

/** @brief Gets current system mode. */
SystemState_t CoreState_GetMode();

/** @brief Gets current network state. */
NetworkState_t CoreState_GetNetwork();

/** @brief Gets current web server state. */
WebServerState_t CoreState_GetWebServer();

/** @brief Gets current MQTT client state. */
MqttState_t CoreState_GetMqtt();

/** @brief Gets flash storage health status. */
bool CoreState_GetStorageStatus();

/** @brief Sets system mode context. */
void CoreState_SetMode(SystemState_t mode);

/** @brief Sets network state. */
void CoreState_SetNetwork(NetworkState_t state);

/** @brief Sets web server state. */
void CoreState_SetWebServer(WebServerState_t state);

/** @brief Sets MQTT client state. */
void CoreState_SetMqtt(MqttState_t state);

/** @brief Sets flash storage health status. */
void CoreState_SetStorageStatus(bool is_ok);

/** @brief Blocks calling task until target subsystem state is reached. */
bool waiting_on_event(Event_t type, uint8_t expected_state, TickType_t timeout_ticks);

/** @brief Bootstrapping entry point for Core Engine framework. */
void CoreEngine_Start();


#endif // CORE_ENGINE_H
