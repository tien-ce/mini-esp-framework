#ifndef WIFI_TASK_H
#define WIFI_TASK_H

#include <Arduino.h>
#include <WiFi.h>
#include "core/config_manager.h"

/** @brief Gets current WiFi SSID. */
String getWifiSSID();

/** @brief Gets current WiFi password. */
String getWifiPassword();

/** @brief Gets device Client ID / Hostname. */
String getWifiClientID();

/** @brief Gets current static IP setting. */
String getWifiIP();

/** @brief Gets current static Gateway setting. */
String getWifiGateway();

/** @brief Gets current static Subnet mask setting. */
String getWifiSubnet();

/** @brief Gets current static DNS1 server setting. */
String getWifiDNS1();

/** @brief Updates WiFi SSID and password configuration settings. */
void updateWifiConfig(const String &newSsid, const String &newPass);

/** @brief Updates device Client ID / Hostname configuration setting. */
void updateWifiClientID(const String &newClientId);

/** @brief Updates WiFi static IP configuration settings. */
void updateWifiStaticIPConfig(const String &newIp, const String &newGw, const String &newSn, const String &newDns);

/** @brief Checks if WiFi is connected. */
bool is_wifi_connected();

/** @brief Gets current WiFi link status. */
wl_status_t get_wifi_link_status();

/** @brief Gets WiFi RSSI signal strength. */
int get_wifi_rssi();

/** @brief FreeRTOS task for WiFi monitoring and auto-reconnection. */
void vWifiTask(void *pvParameters);

#endif // WIFI_TASK_H



