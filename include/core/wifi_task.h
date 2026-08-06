#ifndef WIFI_TASK_H
#define WIFI_TASK_H

#include <Arduino.h>
#include <WiFi.h>
#include "core/config_manager.h"

/** @brief Gets current WiFi SSID. */
String getWifiSSID();

/** @brief Gets current WiFi password. */
String getWifiPassword();

/** @brief Updates WiFi configuration settings. */
void updateWifiConfig(const String &newSsid, const String &newPass);

/** @brief Checks if WiFi is connected. */
bool is_wifi_connected();

/** @brief Gets current WiFi link status. */
wl_status_t get_wifi_link_status();

/** @brief Gets WiFi RSSI signal strength. */
int get_wifi_rssi();

/** @brief FreeRTOS task for WiFi monitoring and auto-reconnection. */
void vWifiTask(void *pvParameters);

#endif // WIFI_TASK_H



