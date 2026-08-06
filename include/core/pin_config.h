#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H
#include <Arduino.h>

#define MAX_GPIO_PINS 48
#define GPIO_INVALID -1

struct PinMap {
    int8_t gpio;
    String name;
};

/** @brief Initializes Pin Config system. */
void pin_config_init(void);

/** @brief Saves RAM pin mappings back to config file. */
void pin_config_save(void);

/** @brief Checks if a GPIO pin is in use. */
bool is_pin_used(int8_t gpio);

/** @brief Gets configured name assigned to GPIO pin. */
String get_pin_name(int8_t gpio);

/** @brief Assigns or updates name/driver for a GPIO pin. */
bool set_pin_name(int8_t gpio, const String& name);

#endif // PIN_CONFIG_H
