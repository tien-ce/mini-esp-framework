#ifndef ESP_INFO_PROVIDER_H
#define ESP_INFO_PROVIDER_H

#include <stdint.h>
#include <stdbool.h>
#include "board.h"  /* BOARD_NAME, written to fw_desc_t.board */

#define CHIP_MODEL_STR_MAX_LEN 16
#define MAC_STR_LEN            18
#define FW_VERSION_STR         "3.1.1"

/* Firmware descriptor: identifies a mini-esp-framework image (see fw_desc_t) */
#define FW_DESC_MAGIC          0x4D455346u    /* "FSEM" in memory: marks that the descriptor is present */
#define FW_DESC_NAME           "MINI_ESP_FW"  /* Same in every version: what OTA compares */
#define FW_DESC_NAME_LEN       16
#define FW_DESC_VERSION_LEN    16
#define FW_DESC_BOARD_LEN      16
#define FW_DESC_BIN_OFFSET     288            /* Position in firmware.bin: image header 24 + segment header 8 + esp_app_desc_t 256 */

/**
 * @brief Descriptor placed right after esp_app_desc_t, at FW_DESC_BIN_OFFSET of firmware.bin.
 * @note Old firmware reads this from new images during OTA: never move or resize a field,
 *       only append new fields at the end.
 */
typedef struct {
    uint32_t magic;                         /* FW_DESC_MAGIC */
    char     name[FW_DESC_NAME_LEN];        /* FW_DESC_NAME */
    char     version[FW_DESC_VERSION_LEN];  /* FW_VERSION_STR of this build */
    char     board[FW_DESC_BOARD_LEN];      /* BOARD_NAME: an image only installs on the same board */
} fw_desc_t;

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

/** @brief Gets the descriptor of the running firmware (the one stored in its own image). */
const fw_desc_t* esp_info_get_fw_desc(void);

/** @brief Gets system uptime in seconds. */
uint32_t esp_info_get_uptime_sec(void);

/** @brief Gets free heap memory in bytes. */
uint32_t esp_info_get_free_heap(void);

/** @brief Gets total heap size in bytes. */
uint32_t esp_info_get_total_heap(void);

/** @brief Gets the lowest free heap size seen since boot, in bytes. */
uint32_t esp_info_get_min_free_heap(void);

/** @brief Gets the largest contiguous free block of internal RAM in bytes (PSRAM excluded; low value = fragmented heap). */
uint32_t esp_info_get_max_alloc_heap(void);

/** @brief Gets the internal chip temperature in degrees Celsius. */
float esp_info_get_chip_temp(void);

/** @brief Gets why the last reset happened ("POWERON", "BROWNOUT", "PANIC", "TASK_WDT", "SOFTWARE", ...). */
const char* esp_info_get_reset_reason(void);

/** @brief True when the firmware is built with PSRAM support (SYSTEM_USES_PSRAM) and the PSRAM was found at boot. */
bool esp_info_has_psram(void);

/** @brief Gets free PSRAM in bytes (0 when the board has none). */
uint32_t esp_info_get_free_psram(void);

/** @brief Gets total PSRAM in bytes (0 when the board has none). */
uint32_t esp_info_get_total_psram(void);

/** @brief Gets flash chip size in bytes. */
uint32_t esp_info_get_flash_size(void);

/** @brief Gets ESP-IDF SDK version string. */
const char* esp_info_get_sdk_version(void);

#endif // ESP_INFO_PROVIDER_H
