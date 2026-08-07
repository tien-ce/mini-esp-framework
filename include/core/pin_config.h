#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

#include <Arduino.h>

/**
 * @file pin_config.h
 * @brief GPIO Pin Configuration Management API.
 * 
 * LittleFS File Storage Format (/pin_config.txt):
 * -----------------------------------------------
 * Line-separated key-value text file format:
 * 
 * GPIO<pin_number>: <assigned_function_or_driver_name>
 * 
 * Example:
 *   GPIO0: Relay
 *   GPIO47: E3FR2C1_IN
 */

#define MAX_GPIO_PINS 48
#define GPIO_INVALID  -1

/* -------------------------------------------------------------------------- */
/*                            PIN OPTIONS & DEFINITIONS                        */
/* -------------------------------------------------------------------------- */


static const char* AVAILABLE_PIN_OPTIONS[] = {
    "None",
    "User",
    "Button",
    "Relay",
    "PWM / LED",
    "I2C SDA",
    "I2C SCL",
    "ADC Input",
    "E3FR2C1_IN"
};
#define AVAILABLE_PIN_OPTIONS_COUNT (sizeof(AVAILABLE_PIN_OPTIONS) / sizeof(AVAILABLE_PIN_OPTIONS[0]))

struct BoardPinDef {
    int8_t gpio;
    const char* label;
    const char* defaultOption;
    bool isFixed;
};

static const BoardPinDef BOARD_PINS[] = {
    {0,  "GPIO0",  "None",        false},
    {1,  "GPIO1",  "None",        false},
    {2,  "GPIO2",  "None",        false},
    {3,  "GPIO3",  "None",        false},
    {4,  "GPIO4",  "None",        false},
    {5,  "GPIO5",  "None",        false},
    {9,  "GPIO9",  "SPI Flash",   true},
    {10, "GPIO10", "SPI Flash",   true},
    {12, "GPIO12", "None",        false},
    {13, "GPIO13", "None",        false},
    {14, "GPIO14", "None",        false},
    {15, "GPIO15", "None",        false},
    {16, "GPIO16", "None",        false},
    {17, "GPIO17", "None",        false},
    {47, "GPIO47", "E3FR2C1_IN",  false}
};
#define BOARD_PIN_COUNT (sizeof(BOARD_PINS) / sizeof(BOARD_PINS[0]))

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

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
