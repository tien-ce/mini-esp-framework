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
    "Relay1",
    "Relay2",
    "Relay3",
    "Relay4",
    "PWM / LED",
    "I2C SDA",
    "I2C SCL",
    "ADC Input",
    "E3FR2C1_IN",
    "RS485_TX",
    "RS485_RX",
    "RS485_DE",
};
#define AVAILABLE_PIN_OPTIONS_COUNT (sizeof(AVAILABLE_PIN_OPTIONS) / sizeof(AVAILABLE_PIN_OPTIONS[0]))

struct BoardPinDef {
    int8_t gpio;
    const char* label;
    const char* defaultOption;
    bool isFixed;
};

#if defined(BOARD_ESP32S3_PLC_MINI)
static const BoardPinDef BOARD_PINS[] = {
    {6, "GPIO6", "Relay1",false},
    {7, "GPIO7", "Relay2",false},
    {15, "GPIO15", "Relay3",false},
    {16, "GPIO16", "Relay4",false},
    /* Default RS485 Pins for esp32-rs485-can*/
    {17, "GPIO17", "RS485_TX",false},
    {18, "GPIO18", "RS485_RX",false},
    {37, "GPIO37", "None", false},
    {38, "GPIO38", "None", false},
    {39, "GPIO39", "None", false},
    {40, "GPIO40", "None", false},
    {41, "GPIO41", "None", false},
    {42, "GPIO42", "None", false},
    {43, "GPIO43", "None", false},
    {44, "GPIO44", "None", false},
    {45, "GPIO45", "None", false},
    {46, "GPIO46", "None", false},
    {47, "GPIO47", "None", false},
    {48, "GPIO48", "None", false},
    {49, "GPIO49", "None", false},
};
#elif defined(BOARD_ESP32S3_RS485_CAN)
static const BoardPinDef BOARD_PINS[] = {
    {0,  "GPIO0",  "None",        false},
    {1,  "GPIO1",  "None",        false},
    {2,  "GPIO2",  "None",        false},
    {3,  "GPIO3",  "None",        false},
    {4,  "GPIO4",  "None",        false},
    {5,  "GPIO5",  "None",        false},
    {6,  "GPIO6",  "None",        false},
    {7,  "GPIO7",  "None",        false},
    {8,  "GPIO8",  "None",        false},
    {9,  "GPIO9",  "SPI Flash",   true},
    {10, "GPIO10", "SPI Flash",   true},
    {12, "GPIO12", "None",        false},
    {13, "GPIO13", "None",        false},
    {14, "GPIO14", "None",        false},
    {15, "GPIO15", "None",        false},
    {16, "GPIO16", "None",        false},
    /* Default RS485 Pins for esp32-rs485-can*/
    {17, "GPIO17", "RS485_TX",    false},
    {18, "GPIO18", "RS485_RX",    false},
    {21, "GPIO21", "RS485_DE",    false},
};
#endif

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
