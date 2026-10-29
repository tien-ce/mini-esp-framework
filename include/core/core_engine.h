#ifndef CORE_ENGINE_H
#define CORE_ENGINE_H

#include <stdbool.h>
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

/* Core Headers (No service includes allowed here) */
#include "core/core_nvs.h"
#include "core/dispatcher.h"
#include "core/info.h"
#include "core/pin_config.h"
#include "core/core_log.h"

// ============================================================================
// 1. GENERIC STATE MANAGEMENT INTERFACE (HANDLE-BASED)
// ============================================================================

/**
 * @brief Sets a mutually exclusive state on a target EventGroup.
 * Clears all previous state bits (0x00FFFFFF) and sets the bit (1 << state).
 *
 * @param group Target EventGroup handle.
 * @param state State index (0 to 23).
 */
void core_state_set(EventGroupHandle_t group, uint8_t state);

/**
 * @brief Blocks calling task until target state bit is active in target EventGroup.
 * Does NOT clear the bit on exit, allowing multiple tasks to observe the state.
 *
 * @param group Target EventGroup handle.
 * @param state Expected state index (0 to 23).
 * @param timeout Maximum FreeRTOS ticks to wait.
 * @return true if state was reached, false on timeout or invalid group.
 */
bool core_state_wait(EventGroupHandle_t group, uint8_t state, TickType_t timeout);

/**
 * @brief Checks if a specific state bit is currently active in target EventGroup.
 *
 * @param group Target EventGroup handle.
 * @param state State index (0 to 23).
 * @return true if active, false otherwise.
 */
bool core_state_is(EventGroupHandle_t group, uint8_t state);

// ============================================================================
// 2. SYSTEM LIFECYCLE DOMAIN (CORE-OWNED)
// ============================================================================

/**
 * @brief System Operational Mode
 * Represents the high-level operating context of the device.
 */
typedef enum {
    SYS_BOOT = 0,         /**< System powering on; queues and heap allocating */
    SYS_SETUP,            /**< Reading NVS configs & starting initial core tasks */
    SYS_NORMAL,           /**< Standard production execution mode, after at least serial start */
    SYS_PROVISIONING,     /**< AP/Setup mode waiting for credentials */
    SYS_OTA_UPDATE,       /**< Firmware update in progress */
    SYS_SAFE_MODE,        /**< Hardware failure or critical config missing */
    SYS_MAX_STATE
} SystemState_t;

/**
 * @brief EventGroup handle for System Lifecycle states.
 * Owned and initialized by Core Engine.
 */
extern EventGroupHandle_t g_system_state_group;

/** @brief Sets system mode context and updates g_system_state_group. */
void CoreState_SetMode(SystemState_t mode);

/** @brief Gets current system mode. */
SystemState_t CoreState_GetMode();

/** @brief Bootstrapping entry point for Core Engine framework. */
void CoreEngine_Start();

#endif // CORE_ENGINE_H
