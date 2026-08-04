#include "core/chip_info.h"
#include "core/log_task.h"
#include <stdio.h>
#include <string.h>
#include <Arduino.h>
#include "esp_chip_info.h"
#include "esp_mac.h"

/* Logging Helper Macros */
#define LOG(msg)         logPrint(String(msg), LOG_LEVEL_INFO);
#define LOG_DEBUG(msg)   logPrint(String(msg), LOG_LEVEL_DEBUG);
#define LOG_INFO(msg)    logPrint(String(msg), LOG_LEVEL_INFO);
#define LOG_WARNING(msg) logPrint(String(msg), LOG_LEVEL_WARNING);
#define LOG_ERROR(msg)   logPrint(String(msg), LOG_LEVEL_ERROR);

// Internal encapsulated instance
static esp_device_info_t s_device_info = {
    "UNKNOWN", // model
    0,         // cores
    0,         // revision
    0,         // features
    {0},       // mac
    false      // is_loaded
};

static const char* parse_chip_model(esp_chip_model_t model) {
    switch (model) {
        case CHIP_ESP32:   return "ESP32";
        case CHIP_ESP32S2: return "ESP32-S2";
        case CHIP_ESP32S3: return "ESP32-S3";
        case CHIP_ESP32C3: return "ESP32-C3";
        default:           return "UNKNOWN";
    }
}

void esp_info_load(void) {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    snprintf(s_device_info.model, sizeof(s_device_info.model), "%s", parse_chip_model(chip_info.model));
    s_device_info.cores = chip_info.cores;
    s_device_info.revision = chip_info.revision;
    s_device_info.features = chip_info.features;

    esp_efuse_mac_get_default(s_device_info.mac);
    s_device_info.is_loaded = true;
    
    LOG_INFO("Hardware information successfully loaded into internal state");
}

const esp_device_info_t* esp_info_get_all(void) {
    if (!s_device_info.is_loaded) {
        LOG_ERROR("esp_info module not initialized. Call esp_info_load() first.");
        return NULL;
    }
    return &s_device_info;
}

const char* esp_info_get_model(void) {
    if (!s_device_info.is_loaded) {
        LOG_ERROR("esp_info module not initialized. Call esp_info_load() first.");
        return s_device_info.model;
    }
    return s_device_info.model;
}

uint8_t esp_info_get_cores(void) {
    if (!s_device_info.is_loaded) {
        LOG_ERROR("esp_info module not initialized. Call esp_info_load() first.");
        return 0;
    }
    return s_device_info.cores;
}

uint16_t esp_info_get_revision(void) {
    if (!s_device_info.is_loaded) {
        LOG_ERROR("esp_info module not initialized. Call esp_info_load() first.");
        return 0;
    }
    return s_device_info.revision;
}

void esp_info_get_mac_bytes(uint8_t dest_mac[MAC_ADDR_LEN]) {
    if (!s_device_info.is_loaded) {
        LOG_ERROR("esp_info module not initialized. Call esp_info_load() first.");
        memset(dest_mac, 0, MAC_ADDR_LEN);
        return;
    }
    memcpy(dest_mac, s_device_info.mac, MAC_ADDR_LEN);
}

void esp_info_get_mac_str(char dest_str[MAC_STR_LEN]) {
    if (!s_device_info.is_loaded) {
        LOG_ERROR("esp_info module not initialized. Call esp_info_load() first.");
        snprintf(dest_str, MAC_STR_LEN, "00:00:00:00:00:00");
        return;
    }
    snprintf(dest_str, MAC_STR_LEN, "%02X:%02X:%02X:%02X:%02X:%02X",
             s_device_info.mac[0], s_device_info.mac[1], s_device_info.mac[2],
             s_device_info.mac[3], s_device_info.mac[4], s_device_info.mac[5]);
}
