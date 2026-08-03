#ifndef WIFI_TASK_H
#define WIFI_TASK_H

#include <Arduino.h>
#include <WiFi.h>
#include "core/config_manager.h"

// Static IP Network Constants
#define USE_STATIC_IP true
#define STATIC_IP "192.168.3.245"
#define STATIC_GATEWAY "192.168.1.1"
#define STATIC_SUBNET "255.255.252.0"
#define STATIC_DNS1 "8.8.8.8"

/**
 * @brief Loads WiFi module configuration from LittleFS wifi_config.txt file.
 * Registers "wifi" module with config_manager if not already registered.
 * @param None
 * @return None
 */
void loadWifiConfig();

/**
 * @brief Saves current in-memory WiFi configuration parameters to LittleFS.
 * @param None
 * @return None
 */
void saveWifiConfig();

/**
 * @brief Gets current WiFi SSID in a thread-safe manner.
 * @param None
 * @return String containing current WiFi SSID.
 */
String getWifiSSID();

/**
 * @brief Gets current WiFi password in a thread-safe manner.
 * @param None
 * @return String containing current WiFi password.
 */
String getWifiPassword();

/**
 * @brief Updates WiFi configuration settings and persists them to LittleFS.
 * @param newSsid New WiFi SSID name.
 * @param newPass New WiFi password.
 * @return None
 */
void updateWifiConfig(const String &newSsid, const String &newPass);

/**
 * @brief Wrapper for updateWifiConfig for backward compatibility.
 * @param newSsid New WiFi SSID name.
 * @param newPass New WiFi password.
 * @return None
 */
void updateConfig(const String &newSsid, const String &newPass);

/**
 * @brief Public utility method for external drivers to check WiFi connection status.
 * @param None
 * @return true if WiFi status is WL_CONNECTED, false otherwise.
 */
bool is_wifi_connected();

/**
 * @brief Public utility method for external drivers to get current WiFi link status.
 * @param None
 * @return wl_status_t Current WiFi link status enum value.
 */
wl_status_t get_wifi_link_status();

/**
 * @brief Public utility method for external drivers to get WiFi RSSI signal strength.
 * @param None
 * @return int Current RSSI value in dBm.
 */
int get_wifi_rssi();

/**
 * @brief Configures WiFi hardware mode, static IP, events, and initiates connection.
 * @param None
 * @return None
 */
void setup_wifi();

/**
 * @brief WiFi station disconnect event callback handler.
 * @param event Arduino WiFi event descriptor.
 * @param info Additional information regarding disconnect event reason.
 * @return None
 */
void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);

/**
 * @brief FreeRTOS task responsible for WiFi link monitoring and auto-reconnection.
 * @param pvParameters FreeRTOS task parameters pointer.
 * @return None
 */
void vWifiTask(void *pvParameters);

#endif // WIFI_TASK_H



