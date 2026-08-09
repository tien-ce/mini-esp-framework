#include "core/core_engine.h"
#include "core/pin_config.h"
/* -------------------------------------------------------------------------- */
/*                             DEFINES & CONSTANTS                            */
/* -------------------------------------------------------------------------- */
#define MAX_OFFSET GetDomainOffset((Event_t)(MAX_EVENT - 1))	// Max valid offset
#define GET_DOMAIN_MASK(type)	((1UL << (GetDomainNumState(type))) - 1)	// Mask for count bits (2 ^ num_state - 1)

/* -------------------------------------------------------------------------- */
/*                              STATIC VARIABLES                              */
/* -------------------------------------------------------------------------- */
static CoreSystemStateMatrix_t  g_state_matrix;
static SemaphoreHandle_t        g_state_mutex = NULL;
static EventGroupHandle_t       g_state_event_group = NULL;

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */
/** @brief Get the number of state of domain event type */
static uint8_t GetDomainNumState(Event_t type) {
	switch(type) {
		case SYSTEM_EVENT:	return	SYS_MAX_STATE;
		case NETWORK_EVENT:	return	NET_MAX_STATE;
		case WEB_EVENT:		return	WEB_MAX_STATE;
		case MQTT_EVENT:	return	MQTT_MAX_STATE;
		default:		return	0;
	}
}

/** @brief Gets bit offset in event group for domain event type. */
static uint8_t GetDomainOffset(Event_t type) {
    switch (type) {
        case SYSTEM_EVENT:  return 0;
        case NETWORK_EVENT: return SYS_MAX_STATE;
        case WEB_EVENT:     return SYS_MAX_STATE + NET_MAX_STATE;
        case MQTT_EVENT:    return SYS_MAX_STATE + NET_MAX_STATE + WEB_MAX_STATE;
        default:            return 0xFF;
    }
}

/** @brief Updates domain bits in state event group dynamically. */
static void UpdateDomainBits(Event_t type, uint8_t new_state) {
    uint8_t offset = GetDomainOffset(type);
    if (offset > MAX_OFFSET) return;

    // 1. Clear all 4 bits allocated for this domain
    xEventGroupClearBits(g_state_event_group, (GET_DOMAIN_MASK(type)<< offset));

    // 2. Set bit corresponding to new_state
    EventBits_t bit_to_set = (EventBits_t)(1 << (offset + new_state));
    xEventGroupSetBits(g_state_event_group, bit_to_set);
}

/** @brief Initializes state matrix mutex and event group. */
static bool CoreState_Init(void) {
    // 1. Allocate FreeRTOS synchronization primitives
    g_state_mutex = xSemaphoreCreateMutex();
    g_state_event_group = xEventGroupCreate();

    if (g_state_mutex == NULL || g_state_event_group == NULL)
        return false;
    // 2. Set initial memory state
    g_state_matrix.mode = SYS_BOOT;
    g_state_matrix.network = NET_STATE_DISCONNECTED;
    g_state_matrix.web = WEB_STATE_STOPPED;
    g_state_matrix.mqtt = MQTT_STATE_DISCONNECTED;
    g_state_matrix.storage_ok = false;
    g_state_matrix.last_update = xTaskGetTickCount();
    // 3. Init config manager (loads registry list from LittleFS) for other modules to use
    config_manager_init();
    // 4. Init pin config system (loads pin_config.txt from LittleFS)
    pin_config_init();

    //5 broadcast MODBE_BOOT event
    CoreState_SetMode(SYS_BOOT);
    return true;
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

bool waiting_on_event(Event_t type, uint8_t expected_state, TickType_t timeout_ticks) {
    if (g_state_event_group == NULL) return false;

    uint8_t offset = GetDomainOffset(type);
    if (offset == 0xFF || expected_state > 15) return false;

    // Calculate exact target bit position
    EventBits_t target_bit = (1 << (offset + expected_state));

    // Block calling thread until bit is set
    EventBits_t result = xEventGroupWaitBits(
        g_state_event_group,
        target_bit,
        pdFALSE,        // Do not clear bit on exit
        pdTRUE,         // Wait for bit to set
        timeout_ticks
    );

    return (result & target_bit) != 0;
}

void CoreState_Get(CoreSystemStateMatrix_t* p_out_state) {
    if (p_out_state == NULL) return;
    if (g_state_mutex != NULL && xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        *p_out_state = g_state_matrix;
        xSemaphoreGive(g_state_mutex);
    }
}

SystemState_t CoreState_GetMode() {
    SystemState_t val = SYS_BOOT;
    if (g_state_mutex != NULL && xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        val = g_state_matrix.mode;
        xSemaphoreGive(g_state_mutex);
    }
    return val;
}

NetworkState_t CoreState_GetNetwork() {
    NetworkState_t val = NET_STATE_DISCONNECTED;
    if (g_state_mutex != NULL && xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        val = g_state_matrix.network;
        xSemaphoreGive(g_state_mutex);
    }
    return val;
}

WebServerState_t CoreState_GetWebServer() {
    WebServerState_t val = WEB_STATE_STOPPED;
    if (g_state_mutex != NULL && xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        val = g_state_matrix.web;
        xSemaphoreGive(g_state_mutex);
    }
    return val;
}

MqttState_t CoreState_GetMqtt() {
    MqttState_t val = MQTT_STATE_DISCONNECTED;
    if (g_state_mutex != NULL && xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        val = g_state_matrix.mqtt;
        xSemaphoreGive(g_state_mutex);
    }
    return val;
}

bool CoreState_GetStorageStatus() {
    bool val = false;
    if (g_state_mutex != NULL && xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        val = g_state_matrix.storage_ok;
        xSemaphoreGive(g_state_mutex);
    }
    return val;
}

void CoreState_SetMode(SystemState_t mode) {
    if (g_state_mutex != NULL && xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        g_state_matrix.mode = mode;
        g_state_matrix.last_update = xTaskGetTickCount();
        xSemaphoreGive(g_state_mutex);
    }
    UpdateDomainBits(SYSTEM_EVENT, (uint8_t)mode);
}

void CoreState_SetNetwork(NetworkState_t state) {
    if (g_state_mutex != NULL && xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        g_state_matrix.network = state;
        g_state_matrix.last_update = xTaskGetTickCount();
        xSemaphoreGive(g_state_mutex);
    }
    UpdateDomainBits(NETWORK_EVENT, (uint8_t)state);
}

void CoreState_SetWebServer(WebServerState_t state) {
    if (g_state_mutex != NULL && xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        g_state_matrix.web = state;
        g_state_matrix.last_update = xTaskGetTickCount();
        xSemaphoreGive(g_state_mutex);
    }
    UpdateDomainBits(WEB_EVENT, (uint8_t)state);
}

void CoreState_SetMqtt(MqttState_t state) {
    if (g_state_mutex != NULL && xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        g_state_matrix.mqtt = state;
        g_state_matrix.last_update = xTaskGetTickCount();
        xSemaphoreGive(g_state_mutex);
    }
    UpdateDomainBits(MQTT_EVENT, (uint8_t)state);
}

void CoreState_SetStorageStatus(bool is_ok) {
    if (g_state_mutex != NULL && xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        g_state_matrix.storage_ok = is_ok;
        g_state_matrix.last_update = xTaskGetTickCount();
        xSemaphoreGive(g_state_mutex);
    }
}

void CoreEngine_Start() {
    // Initialize state mutex & event group FIRST: vLogTask (created below) touches
    // g_state_event_group as soon as it runs, and it can preempt this task the
    // instant it's created (higher priority, same core). If the event group didn't
    // exist yet, waiting_on_event() would bail out immediately instead of blocking.
    if (!CoreState_Init())
      return;
    /* Starting intialize task */
    CoreState_SetMode(SYS_SETUP);
    // Task: Log & Command Processing Task (Priority 2)
    xTaskCreatePinnedToCore(
        vLogTask,
        "LogTask",
        4096,
        NULL,
        2,
        NULL,
        1
    );
    // Task: WiFi State Monitoring & Auto-Reconnect Task (Priority 2)
    xTaskCreatePinnedToCore(
        vWifiTask,
        "WifiTask",
        4096,
        NULL,
        2,
        NULL,
        1
    );

    // Task: Web Server & System Monitor Task (Priority 1)
    xTaskCreatePinnedToCore(
       vWebMonitorTask,
       "WebMonitorTask",
       4096,
       NULL,
       1,
       NULL,
       1
    );

    
    waiting_on_event(SYSTEM_EVENT, SYS_NORMAL, pdMS_TO_TICKS(5000));
}
