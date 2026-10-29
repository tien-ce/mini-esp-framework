#ifndef ESP_INFO_PROVIDER_H
#define ESP_INFO_PROVIDER_H

#include <stdint.h>
#include <stdbool.h>

#define CHIP_MODEL_STR_MAX_LEN 16
#define MAC_STR_LEN            18
#define FW_VERSION_STR         "3.1.0"

/**
 * @brief Structure storing relevant web display information.
 */
typedef struct {
    char model[CHIP_MODEL_STR_MAX_LEN];
    char mac_str[MAC_STR_LEN];
    bool is_loaded;
} esp_device_info_t;

/** @brief Gets chip model string. */
const char* esp_info_get_model(void);

/** @brief Gets short chip label ("S3", "C3", "ESP32"), pointing into the cached model string. */
const char* esp_info_get_chip_short(void);

/** @brief Gets formatted MAC address string. */
const char* esp_info_get_mac_str(void);

/** @brief Gets static firmware version string. */
const char* esp_info_get_fw_version(void);

/** @brief Gets system uptime in seconds. */
uint32_t esp_info_get_uptime_sec(void);

/** @brief Gets free heap memory in bytes. */
uint32_t esp_info_get_free_heap(void);

/** @brief Gets total heap size in bytes. */
uint32_t esp_info_get_total_heap(void);

/** @brief Gets the lowest free heap size seen since boot, in bytes. */
uint32_t esp_info_get_min_free_heap(void);

/** @brief Gets free PSRAM in bytes (0 when the board has none). */
uint32_t esp_info_get_free_psram(void);

/** @brief Gets total PSRAM in bytes (0 when the board has none). */
uint32_t esp_info_get_total_psram(void);

/** @brief Gets flash chip size in bytes. */
uint32_t esp_info_get_flash_size(void);

/** @brief Gets ESP-IDF SDK version string. */
const char* esp_info_get_sdk_version(void);

#endif // ESP_INFO_PROVIDER_H
