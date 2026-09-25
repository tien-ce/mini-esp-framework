#ifndef WIFI_TASK_H
#define WIFI_TASK_H

#include <Arduino.h>
#include <WiFi.h>
#include "core/core_nvs.h"

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

/**
 * @brief FreeRTOS task quản lý cấu hình mạng WiFi và máy trạng thái tự động kết nối lại non-blocking.
 * 
 * @details Task khởi tạo kết nối WiFi ban đầu (chế độ STA, static IP nếu bật), đăng ký các lệnh CLI,
 *          và chạy vòng lặp giám sát định kỳ. Khi phát hiện mất kết nối, task kích hoạt máy trạng thái
 *          kết nối lại tự động với chu kỳ non-blocking 30 giây mà không làm treo các tiến trình hệ thống khác.
 * 
 * @param[in] pvParameters Con trỏ tham số truyền vào từ FreeRTOS xTaskCreate (không sử dụng, có thể là NULL).
 */
void vWifiTask(void *pvParameters);

#endif // WIFI_TASK_H



