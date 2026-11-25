#include "services/wifi/wifi_state.h"
#include "core/core_engine.h"

EventGroupHandle_t g_wifi_state_group = NULL;
static volatile WifiState_t s_wifi_state = WIFI_STATE_DISCONNECTED;

void wifi_state_init() {
    if (g_wifi_state_group == NULL) {
        g_wifi_state_group = xEventGroupCreate();
        if (g_wifi_state_group != NULL) {
            core_state_set(g_wifi_state_group, (uint8_t)WIFI_STATE_DISCONNECTED);
        }
    }
}

void wifi_state_set(WifiState_t state) {
    s_wifi_state = state;
    if (g_wifi_state_group != NULL) {
        core_state_set(g_wifi_state_group, (uint8_t)state);
    }
}

WifiState_t get_wifi_state() {
    return s_wifi_state;
}

bool is_wifi_sta_connected() {
    return (s_wifi_state == WIFI_STATE_STA);
}
