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

    register_config_file("pin_config", "pin_config.txt");
    String raw = read_config("pin_config");

    if (raw.length() == 0) {
        LOG_INFO("pin_config.txt empty or missing. Populating default pin mappings.");
        
        // 1. Iterate through predefined board pin definitions and assign non-None defaults to RAM table
        for (size_t i = 0; i < BOARD_PIN_COUNT; i++) {
            const auto& p = BOARD_PINS[i];
            if (!p.isFixed && String(p.defaultOption) != "None" && p.gpio >= 0 && p.gpio < MAX_GPIO_PINS) {
                g_pin_names[p.gpio] = p.defaultOption;
            }
        }
        
        // 2. Format assigned default pin configurations into "GPIOx: name" line entries
        String content = "";
        for (int i = 0; i < MAX_GPIO_PINS; i++) {
            if (g_pin_names[i].length() > 0) {
                content += "GPIO" + String(i) + ": " + g_pin_names[i] + "\n";
            }
        }
        
        // 3. Save default pin configuration to pin_config.txt on LittleFS
        if (content.length() > 0) {
            save_config("pin_config", content);
        }
        return;
    }


    int pos = 0;
    while (pos < raw.length()) {
        int nextPos = raw.indexOf('\n', pos);
        if (nextPos == -1) nextPos = raw.length();
        String line = raw.substring(pos, nextPos);
        line.trim();
        pos = nextPos + 1;

        if (line.length() == 0) continue;

        // Parse line format: "GPIO<number>: <name>" (e.g., "GPIO47: RELAY")
        if (line.startsWith("GPIO")) {

            int colonIdx = line.indexOf(':');
            if (colonIdx > 4) {
                int gpio_num = line.substring(4, colonIdx).toInt();
                String name = line.substring(colonIdx + 1);
                name.trim();

                if (gpio_num >= 0 && gpio_num < MAX_GPIO_PINS) {
                    g_pin_names[gpio_num] = name;
                }
            }
        }
    }
}

void pin_config_save(void) {
    String content = "";
    for (int i = 0; i < MAX_GPIO_PINS; i++) {
        if (g_pin_names[i].length() > 0) {
            content += "GPIO" + String(i) + ": " + g_pin_names[i] + "\n";
        }
    }
    /* Save pin config and restart */
    save_config("pin_config", content);
    postIncomingCommand(CMD_RESTART);
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


