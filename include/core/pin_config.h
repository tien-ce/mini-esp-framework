#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H
#include <Arduino.h>

#define MAX_GPIO_PINS 48
#define GPIO_INVALID -1

struct PinMap {
    int8_t gpio;
    String name;
};

struct PinOption {
    int value;
    const char* name;
};

static const PinOption AVAILABLE_PIN_OPTIONS[] = {
    {0, "None"},
    {1, "User"},
    {2, "Button"},
    {3, "Relay"},
    {4, "PWM / LED"},
    {5, "I2C SDA"},
    {6, "I2C SCL"},
    {7, "ADC Input"}
};
#define AVAILABLE_PIN_OPTIONS_COUNT (sizeof(AVAILABLE_PIN_OPTIONS) / sizeof(AVAILABLE_PIN_OPTIONS[0]))

struct BoardPinDef {
    int8_t gpio;
    const char* label;
    const char* defaultOption;
    bool isFixed;
};

static const BoardPinDef BOARD_PINS[] = {
    {0,  "GPIO0",  "None",      false},
    {1,  "GPIO1",  "None",      false},
    {2,  "GPIO2",  "None",      false},
    {3,  "GPIO3",  "None",      false},
    {4,  "GPIO4",  "None",      false},
    {5,  "GPIO5",  "None",      false},
    {9,  "GPIO9",  "SPI Flash", true},
    {10, "GPIO10", "SPI Flash", true},
    {12, "GPIO12", "None",      false},
    {13, "GPIO13", "None",      false},
    {14, "GPIO14", "None",      false},
    {15, "GPIO15", "None",      false},
    {16, "GPIO16", "None",      false},
    {17, "GPIO17", "None",      false}
};
#define BOARD_PIN_COUNT (sizeof(BOARD_PINS) / sizeof(BOARD_PINS[0]))

/** @brief Initializes Pin Config system. */
void pin_config_init(void);

/** @brief Saves RAM pin mappings back to config file. */
void pin_config_save(void);

/** @brief Checks if a GPIO pin (by number) is assigned. */
bool is_pin_used(int8_t gpio);

/** @brief Checks if a function/driver name is assigned to any GPIO pin. */
bool is_use_name(const String &name);

/** @brief Gets configured function/driver name assigned to a GPIO pin. */
String get_pin_name(int8_t gpio);

/** @brief Assigns or updates name/driver for a GPIO pin. */
bool set_pin_name(int8_t gpio, const String& name);

/** @brief Gets the GPIO pin number assigned to a function/driver name. */
int8_t get_gpio_by_name(const String &name);



#endif // PIN_CONFIG_H
