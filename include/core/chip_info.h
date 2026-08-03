#ifndef ESP_INFO_PROVIDER_H
#define ESP_INFO_PROVIDER_H

#include <stdint.h>
#include <stdbool.h>

#define CHIP_MODEL_STR_MAX_LEN 16
#define MAC_ADDR_LEN           6
#define MAC_STR_LEN            18

/**
 * @brief Structure storing hardware information of the ESP32 device.
 */
typedef struct {
    char model[CHIP_MODEL_STR_MAX_LEN];
    uint8_t cores;
    uint16_t revision;
    uint32_t features;
    uint8_t mac[MAC_ADDR_LEN];
    bool is_loaded;
} esp_device_info_t;

/**
 * @brief Reads eFuse and chip registries, initializing the internal static hardware state.
 * @param None
 * @return None
 */
void esp_info_load(void);

/**
 * @brief Gets a read-only pointer to the internal hardware information structure.
 * @param None
 * @return Pointer to const esp_device_info_t structure, or NULL if not initialized.
 */
const esp_device_info_t* esp_info_get_all(void);

/**
 * @brief Gets the chip model identifier string (e.g., "ESP32-S3").
 * @param None
 * @return Pointer to model C-string, or "UNKNOWN" if not initialized.
 */
const char* esp_info_get_model(void);

/**
 * @brief Gets the number of active CPU cores on the chip.
 * @param None
 * @return Number of CPU cores, or 0 if not initialized.
 */
uint8_t esp_info_get_cores(void);

/**
 * @brief Gets the hardware silicon revision number.
 * @param None
 * @return Silicon revision, or 0 if not initialized.
 */
uint16_t esp_info_get_revision(void);

/**
 * @brief Copies the 6-byte base MAC address into the provided destination buffer.
 * @param dest_mac Destination buffer array of at least 6 bytes.
 * @return None
 */
void esp_info_get_mac_bytes(uint8_t dest_mac[MAC_ADDR_LEN]);

/**
 * @brief Formats and copies the base MAC address string ("XX:XX:XX:XX:XX:XX").
 * @param dest_str Destination buffer array of at least 18 bytes.
 * @return None
 */
void esp_info_get_mac_str(char dest_str[MAC_STR_LEN]);

#endif // ESP_INFO_PROVIDER_H