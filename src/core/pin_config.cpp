#include "core/pin_config.h"
#include "core/config_manager.h"
#include "core/log_task.h"
static PinMap pin_table[MAX_GPIO_PINS];

void pin_config_init(void) {
    for (int i = 0; i < MAX_GPIO_PINS; i++) {
        pin_table[i].gpio = i;
        pin_table[i].name = "";
    }

    register_config_file("pin_config", "pin_config.txt");
    String raw = read_config("pin_config");

    if (raw.length() == 0) {
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

        // Parse format "GPIOx: name"
        if (line.startsWith("GPIO")) {
            int colonIdx = line.indexOf(':');
            if (colonIdx > 4) {
                int gpio_num = line.substring(4, colonIdx).toInt();
                String name = line.substring(colonIdx + 1);
                name.trim();

                if (gpio_num >= 0 && gpio_num < MAX_GPIO_PINS) {
                    pin_table[gpio_num].name = name;
                }
            }
        }
    }
}

void pin_config_save(void) {
    String content = "";
    for (int i = 0; i < MAX_GPIO_PINS; i++) {
        if (pin_table[i].name.length() > 0) {
            content += "GPIO" + String(i) + ": " + pin_table[i].name + "\n";
        }
    }
    /* Save pin config and restart */
    save_config("pin_config", content);
    postIncomingCommand(CMD_RESTART);
}

bool is_pin_used(int8_t gpio) {
    if (gpio < 0 || gpio >= MAX_GPIO_PINS) return false;
    return pin_table[gpio].name.length() > 0;
}

String get_pin_name(int8_t gpio) {
    if (gpio < 0 || gpio >= MAX_GPIO_PINS) return "";
    return pin_table[gpio].name;
}

bool set_pin_name(int8_t gpio, const String& name) {
    if (gpio < 0 || gpio >= MAX_GPIO_PINS) return false;
    pin_table[gpio].name = name;
    return true;
}
