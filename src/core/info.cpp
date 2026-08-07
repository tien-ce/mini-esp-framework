#include "core/info.h"
#include <Arduino.h>
#include <esp_chip_info.h>
#include <esp_mac.h>
#include <stdio.h>

/* -------------------------------------------------------------------------- */
/*                              STATIC VARIABLES                              */
/* -------------------------------------------------------------------------- */

static esp_device_info_t s_device_info = {
    "UNKNOWN",
    "00:00:00:00:00:00",
    false
};

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initializes and caches hardware static info (Model, MAC).
 */
static void esp_info_load(void) {
    if (s_device_info.is_loaded) {
        return;
    }

    // 1. Get Chip Model
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    switch (chip_info.model) {
        case CHIP_ESP32:
            snprintf(s_device_info.model, sizeof(s_device_info.model), "ESP32");
            break;
        case CHIP_ESP32S2:
            snprintf(s_device_info.model, sizeof(s_device_info.model), "ESP32-S2");
            break;
        case CHIP_ESP32S3:
            snprintf(s_device_info.model, sizeof(s_device_info.model), "ESP32-S3");
            break;
        case CHIP_ESP32C3:
            snprintf(s_device_info.model, sizeof(s_device_info.model), "ESP32-C3");
            break;
        default:
            snprintf(s_device_info.model, sizeof(s_device_info.model), "ESP32-Generic");
            break;
    }

    // 2. Get MAC Address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(s_device_info.mac_str, sizeof(s_device_info.mac_str),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    s_device_info.is_loaded = true;
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

const char* esp_info_get_model(void) {
    if (!s_device_info.is_loaded) esp_info_load();
    return s_device_info.model;
}

const char* esp_info_get_mac_str(void) {
    if (!s_device_info.is_loaded) esp_info_load();
    return s_device_info.mac_str;
}

const char* esp_info_get_fw_version(void) {
    return FW_VERSION_STR;
}

uint32_t esp_info_get_uptime_sec(void) {
    return (uint32_t)(millis() / 1000);
}

uint32_t esp_info_get_free_heap(void) {
    return ESP.getFreeHeap();
}