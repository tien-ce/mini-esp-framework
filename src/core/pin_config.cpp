#include "core/pin_config.h"
#include "core/config_manager.h"
#include "core/log_task.h"

/* -------------------------------------------------------------------------- */
/*                              STATIC VARIABLES                              */
/* -------------------------------------------------------------------------- */

static String g_pin_names[MAX_GPIO_PINS];

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

void pin_config_init(void) {
    for (int i = 0; i < MAX_GPIO_PINS; i++) {
        g_pin_names[i] = "";
    }

    register_config_module("pin_config", "pin_cfg");

    bool has_saved_config = false;
    if (config_get_lock()) {
        for (int i = 0; i < MAX_GPIO_PINS; i++) {
            String key = "gpio" + String(i);
            if (config_has_key("pin_config", key)) {
                g_pin_names[i] = config_read_string("pin_config", key, "");
                if (g_pin_names[i].length() > 0) {
                    has_saved_config = true;
                }
            }
        }
        config_release_lock();
    }

    if (!has_saved_config) {
        LOG_INFO("Pin config missing in NVS. Populating default pin mappings.");
        
        // 1. Iterate through predefined board pin definitions and assign non-None defaults to RAM table
        for (size_t i = 0; i < BOARD_PIN_COUNT; i++) {
            const auto& p = BOARD_PINS[i];
            if (!p.isFixed && String(p.defaultOption) != "None" && p.gpio >= 0 && p.gpio < MAX_GPIO_PINS) {
                g_pin_names[p.gpio] = p.defaultOption;
            }
        }
        
        // 2. Save default pin configuration to NVS
        pin_config_save();
        return;
    }

    LOG_INFO("Pin config loaded from NVS successfully.");
}

void pin_config_save(void) {
    if (config_get_lock()) {
        for (int i = 0; i < MAX_GPIO_PINS; i++) {
            String key = "gpio" + String(i);
            config_save_string("pin_config", key, g_pin_names[i]);
        }
        config_release_lock();
    }
}

bool is_pin_used(int8_t gpio) {
    if (gpio < 0 || gpio >= MAX_GPIO_PINS) return false;
    return g_pin_names[gpio].length() > 0;
}

bool is_use_name(const String &name) {
    if (name.length() == 0) return false;
    for (int i = 0; i < MAX_GPIO_PINS; i++) {
        if (g_pin_names[i].equalsIgnoreCase(name)) {
            return true;
        }
    }
    return false;
}

String get_pin_name(int8_t gpio) {
    if (gpio < 0 || gpio >= MAX_GPIO_PINS) return "";
    return g_pin_names[gpio];
}

int8_t get_gpio_by_name(const String &name) {
    if (name.length() == 0) return GPIO_INVALID;
    for (int i = 0; i < MAX_GPIO_PINS; i++) {
        if (g_pin_names[i].equalsIgnoreCase(name)) {
            return (int8_t)i;
        }
    }
    return GPIO_INVALID;
}

bool set_pin_name(int8_t gpio, const String& name) {
    if (gpio < 0 || gpio >= MAX_GPIO_PINS) return false;
    g_pin_names[gpio] = name;
    return true;
}


