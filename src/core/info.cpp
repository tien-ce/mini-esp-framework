#include "core/info.h"
#include "config.h"
#include <Arduino.h>
#include <esp_chip_info.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

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

const char* esp_info_get_chip_short(void) {
    const char *model = esp_info_get_model();
    /* "ESP32-S3" -> "S3"; plain "ESP32" and the generic fallback stay "ESP32" */
    if (strncmp(model, "ESP32-", 6) == 0 && strcmp(model + 6, "Generic") != 0) {
        return model + 6;
    }
    return "ESP32";
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

uint32_t esp_info_get_total_heap(void) {
    return ESP.getHeapSize();
}

uint32_t esp_info_get_min_free_heap(void) {
    return ESP.getMinFreeHeap();
}

uint32_t esp_info_get_max_alloc_heap(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

float esp_info_get_chip_temp(void) {
    return temperatureRead();
}

const char* esp_info_get_reset_reason(void) {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:   return "POWERON";
        case ESP_RST_EXT:       return "EXTERNAL";
        case ESP_RST_SW:        return "SOFTWARE";
        case ESP_RST_PANIC:     return "PANIC";
        case ESP_RST_INT_WDT:   return "INT_WDT";
        case ESP_RST_TASK_WDT:  return "TASK_WDT";
        case ESP_RST_WDT:       return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_BROWNOUT:  return "BROWNOUT";
        case ESP_RST_SDIO:      return "SDIO";
        default:                return "UNKNOWN";
    }
}

bool esp_info_has_psram(void) {
#ifdef SYSTEM_USES_PSRAM
    return psramFound();
#else
    return false;
#endif
}

uint32_t esp_info_get_free_psram(void) {
#ifdef SYSTEM_USES_PSRAM
    return ESP.getFreePsram();
#else
    return 0;
#endif
}

uint32_t esp_info_get_total_psram(void) {
#ifdef SYSTEM_USES_PSRAM
    return ESP.getPsramSize();
#else
    return 0;
#endif
}

uint32_t esp_info_get_flash_size(void) {
    return ESP.getFlashChipSize();
}

const char* esp_info_get_sdk_version(void) {
    return ESP.getSdkVersion();
}
