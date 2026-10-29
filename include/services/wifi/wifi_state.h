#ifndef WIFI_STATE_H
#define WIFI_STATE_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

/**
 * @brief Layer-2 / Layer-3 Network Connectivity State
 * Managed exclusively by wifi service.
 */
typedef enum {
    WIFI_STATE_DISCONNECTED = 0, /**< Wi-Fi radio off or disconnected from AP */
    WIFI_STATE_CONNECTING,       /**< Actively negotiating link or waiting for DHCP IP */
    WIFI_STATE_STA,              /**< Connected as Station with valid IP assigned */
    WIFI_STATE_AP,               /**< Operating as local Access Point (Fallback / Portal) */
    WIFI_STATE_MAX
} WifiState_t;

/* --- Aliases for backward compatibility during transition --- */
typedef WifiState_t NetworkState_t;
#define NET_STATE_DISCONNECTED WIFI_STATE_DISCONNECTED
#define NET_STATE_CONNECTING   WIFI_STATE_CONNECTING
#define NET_STATE_WIFI_STA     WIFI_STATE_STA
#define NET_STATE_WIFI_AP      WIFI_STATE_AP

/**
 * @brief EventGroup handle for Network Connectivity states.
 * Owned and managed exclusively by wifi_state.
 */
extern EventGroupHandle_t g_wifi_state_group;

/** @brief Initializes the wifi state event group. */
void wifi_state_init();

/** @brief Sets current WiFi state and updates event group bits. */
void wifi_state_set(WifiState_t state);

/** @brief Gets current WiFi state. */
WifiState_t get_wifi_state();

/** @brief Helper to check if WiFi Station is connected. */
bool is_wifi_sta_connected();

#endif // WIFI_STATE_H
