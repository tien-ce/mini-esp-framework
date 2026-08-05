#ifndef ESP_INFO_PROVIDER_H
#define ESP_INFO_PROVIDER_H

#include <stdint.h>
#include <stdbool.h>

#define CHIP_MODEL_STR_MAX_LEN 16
#define MAC_STR_LEN            18
#define FW_VERSION_STR         "1.0.2"

/**
 * @brief Structure storing relevant web display information.
 */
typedef struct {
    char model[CHIP_MODEL_STR_MAX_LEN];
    char mac_str[MAC_STR_LEN];
    bool is_loaded;
} esp_device_info_t;

/**
 * @brief Initializes and caches hardware static info (Model, MAC).
 */
void esp_info_load(void);

/**
 * @brief Gets chip model string (e.g., "ESP32-S3").
 */
const char* esp_info_get_model(void);

/**
 * @brief Formats and gets MAC address string ("XX:XX:XX:XX:XX:XX").
 */
const char* esp_info_get_mac_str(void);

/**
 * @brief Gets static firmware version string.
 */
const char* esp_info_get_fw_version(void);

/**
 * @brief Gets system uptime in seconds (Dynamic).
 */
uint32_t esp_info_get_uptime_sec(void);

/**
 * @brief Gets free heap memory in bytes (Dynamic).
 */
uint32_t esp_info_get_free_heap(void);

#endif // ESP_INFO_PROVIDER_H