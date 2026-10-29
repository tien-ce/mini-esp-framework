#include "core/core_engine.h"
#include "core/file_system.h"
#include "core/pin_config.h"
#include "core/dispatcher.h"
#include "core/core_log.h"

/* -------------------------------------------------------------------------- */
/*                              GLOBAL VARIABLES                              */
/* -------------------------------------------------------------------------- */
EventGroupHandle_t g_system_state_group = NULL;

/* -------------------------------------------------------------------------- */
/*                              STATIC VARIABLES                              */
/* -------------------------------------------------------------------------- */
static SystemState_t     s_system_mode = SYS_BOOT;
static SemaphoreHandle_t s_system_mode_mutex = NULL;

/* -------------------------------------------------------------------------- */
/*                        GENERIC STATE IMPLEMENTATION                        */
/* -------------------------------------------------------------------------- */

void core_state_set(EventGroupHandle_t group, uint8_t state) {
    if (group == NULL || state >= 24) return;
    // Clear all existing state bits in the 24-bit range
    xEventGroupClearBits(group, 0x00FFFFFF);
    // Set the bit corresponding to the new active state
    xEventGroupSetBits(group, (1UL << state));
}

bool core_state_wait(EventGroupHandle_t group, uint8_t state, TickType_t timeout) {
    if (group == NULL || state >= 24) return false;
    EventBits_t target_bit = (1UL << state);
    EventBits_t result = xEventGroupWaitBits(
        group,
        target_bit,
        pdFALSE,        // Do not clear bit on exit (allows multiple observers)
        pdTRUE,         // Wait until the target bit is set
        timeout
    );
    return (result & target_bit) != 0;
}

bool core_state_is(EventGroupHandle_t group, uint8_t state) {
    if (group == NULL || state >= 24) return false;
    EventBits_t bits = xEventGroupGetBits(group);
    return (bits & (1UL << state)) != 0;
}

/* -------------------------------------------------------------------------- */
/*                         SYSTEM MODE & INITIALIZATION                       */
/* -------------------------------------------------------------------------- */

static bool CoreState_Init(void) {
    // 1. Initialize Serial & Logging hardware immediately for early boot visibility
    core_log_init();

    // 2. Allocate System State synchronization primitives
    s_system_mode_mutex = xSemaphoreCreateMutex();
    g_system_state_group = xEventGroupCreate();

    if (s_system_mode_mutex == NULL || g_system_state_group == NULL)
        return false;

    LOG_INFO("=== System Booting (SYS_BOOT) ===");

    // 3. Init config manager (NVS / Preferences)
    core_nvs_init();

    // 4. Init file system  
    if (!file_system_init()) {
        file_system_format();
    }

    // 5. Init pin config system (loads pin mappings from NVS)
    pin_config_init();

    // 6. Init dispatcher
    dispatcher_init();

    // Load saved log level from NVS and apply it to Core Log
    int32_t saved_log_level = core_nvs_read_int("log", "serial_level", 1); // 1 = LOG_LEVEL_INFO
    core_log_set_level((LogLevel)saved_log_level);

    // Broadcast SYS_BOOT state
    CoreState_SetMode(SYS_BOOT);
    return true;
}

void CoreState_SetMode(SystemState_t mode) {
    if (s_system_mode_mutex != NULL && xSemaphoreTake(s_system_mode_mutex, portMAX_DELAY) == pdTRUE) {
        s_system_mode = mode;
        xSemaphoreGive(s_system_mode_mutex);
    }
    core_state_set(g_system_state_group, (uint8_t)mode);
}

SystemState_t CoreState_GetMode() {
    SystemState_t val = SYS_BOOT;
    if (s_system_mode_mutex != NULL && xSemaphoreTake(s_system_mode_mutex, portMAX_DELAY) == pdTRUE) {
        val = s_system_mode;
        xSemaphoreGive(s_system_mode_mutex);
    }
    return val;
}

void CoreEngine_Start() {
    // Initialize system state primitives and core infrastructure
    if (!CoreState_Init())
        return;

    /* Core ready to enter setup mode */
    CoreState_SetMode(SYS_SETUP);
}
