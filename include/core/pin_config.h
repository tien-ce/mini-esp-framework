#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

#include <Arduino.h>
#include "board.h"

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
    "Relay5",
    "Relay6",
    "PWM / LED",
    "I2C SDA",
    "I2C SCL",
    "ADC Input",
    "PHOTO1_IN",
    /* UART */
    "SERIAL_UART1_TX",
    "SERIAL_UART1_RX",
    "SERIAL_UART1_DE",
     
    "SERIAL_UART2_TX",
    "SERIAL_UART2_RX",
    "SERIAL_UART2_DE",
    /* Modbus */
    "SERIAL_MODBUS1_TX",
    "SERIAL_MODBUS1_RX",
    "SERIAL_MODBUS1_DE",

    "SERIAL_MODBUS2_TX",
    "SERIAL_MODBUS2_RX",
    "SERIAL_MODBUS2_DE",
};
#define AVAILABLE_PIN_OPTIONS_COUNT (sizeof(AVAILABLE_PIN_OPTIONS) / sizeof(AVAILABLE_PIN_OPTIONS[0]))

struct BoardPinDef {
    int8_t gpio;
    const char* label;
    const char* defaultOption;
    bool isFixed;
};

#if BOARD == BOARD_PLC_MINI
/* Pinout: docs/hardware/HD SU DUNG BOARD ESP32-S3 PLC MINI.pdf */
static const BoardPinDef BOARD_PINS[] = {
    /* Programming header (UART0 + boot): not for user functions */
    {0,  "GPIO0",  "BOOT",      true},
    {43, "GPIO43", "UART0_TX",  true},
    {44, "GPIO44", "UART0_RX",  true},
    /* Ethernet W5500 (SPI) */
    {5,  "GPIO5",  "ETH_RST",   true},
    {10, "GPIO10", "ETH_CS",    true},
    {11, "GPIO11", "ETH_MOSI",  true},
    {12, "GPIO12", "ETH_SCLK",  true},
    {13, "GPIO13", "ETH_MISO",  true},
    /* 4 transistor outputs, connector OUTPUT pins 1..4 */
    {6,  "GPIO6",  "Relay1",    false},
    {7,  "GPIO7",  "Relay2",    false},
    {15, "GPIO15", "Relay3",    false},
    {16, "GPIO16", "Relay4",    false},
    /* Serial 1: Serial1.begin(baud, config, RX 18, TX 17) */
    {17, "GPIO17", "None", false},
    {18, "GPIO18", "None", false},
    /* 8 inputs, connector INPUT pins 1..8 */
    {47, "GPIO47", "None",      false},
    {48, "GPIO48", "None",      false},
    {45, "GPIO45", "None",      false},
    {35, "GPIO35", "None",      false},
    {36, "GPIO36", "None",      false},
    {37, "GPIO37", "None",      false},
    {38, "GPIO38", "None",      false},
    {39, "GPIO39", "None",      false},
    /* Header 5x2 (GPIO14 and GPIO21 can do PWM; GPIO5 is ETH_RST above) */
    {1,  "GPIO1",  "None",      false},
    {2,  "GPIO2",  "None",      false},
    {4,  "GPIO4",  "None",      false},
    {14, "GPIO14", "None",      false},
    {21, "GPIO21", "None",      false},
    {41, "GPIO41", "None",      false},
    {42, "GPIO42", "None",      false},
};
#elif BOARD == BOARD_PLC_MULTI
/* Pinout: docs/hardware/HD SU DUNG BOARD ESP32-S3 PLC MUTI.pdf */
static const BoardPinDef BOARD_PINS[] = {
    /* Programming port (UART0 + boot): not for user functions */
    {0,  "GPIO0",  "BOOT",         true},
    {43, "GPIO43", "UART0_TX",     true},
    {44, "GPIO44", "UART0_RX",     true},
    /* Ethernet W5500 (SPI + interrupt) */
    {4,  "GPIO4",  "ETH_PHY_IRQ",  true},
    {5,  "GPIO5",  "ETH_RST",      true},
    {10, "GPIO10", "ETH_CS",       true},
    {11, "GPIO11", "ETH_MOSI",     true},
    {12, "GPIO12", "ETH_SCLK",     true},
    {13, "GPIO13", "ETH_MISO",     true},
    /* 6 transistor outputs: GPIO 6, 7 share CM1; GPIO 15, 16, 14, 21 share CM0 */
    {6,  "GPIO6",  "Relay1",       false},
    {7,  "GPIO7",  "Relay2",       false},
    {15, "GPIO15", "Relay3",       false},
    {16, "GPIO16", "Relay4",       false},
    {14, "GPIO14", "Relay5",       false},
    {21, "GPIO21", "Relay6",       false},
    /* Serial 1 (default): Serial1.begin(baud, config, RX 18, TX 17) */
    {17, "GPIO17", "None",   false},
    {18, "GPIO18", "None",   false},
    /* Serial 2 (default): Serial2.begin(baud, config, RX 2, TX 1) */
    {1,  "GPIO1",  "None",   false},
    {2,  "GPIO2",  "None",   false},
    /* Inputs, connector INPUT pins 1..8; pins 4..6 (GPIO35..37) are taken
     * by the octal PSRAM below and must stay cut from the board */
    {47, "GPIO47", "None",         false},
    {48, "GPIO48", "None",         false},
    {45, "GPIO45", "None",         false},
    {38, "GPIO38", "None",         false},
    {39, "GPIO39", "None",         false},
    /* Flash + octal PSRAM bus (N16R8): touching any of these crashes the chip */
    {26, "GPIO26", "Flash/PSRAM",  true},
    {27, "GPIO27", "Flash/PSRAM",  true},
    {28, "GPIO28", "Flash/PSRAM",  true},
    {29, "GPIO29", "Flash/PSRAM",  true},
    {30, "GPIO30", "Flash/PSRAM",  true},
    {31, "GPIO31", "Flash/PSRAM",  true},
    {32, "GPIO32", "Flash/PSRAM",  true},
    {33, "GPIO33", "Flash/PSRAM",  true},
    {34, "GPIO34", "Flash/PSRAM",  true},
    {35, "GPIO35", "Flash/PSRAM",  true},
    {36, "GPIO36", "Flash/PSRAM",  true},
    {37, "GPIO37", "Flash/PSRAM",  true},
    /* Header 5x2 CN1 (GPIO19 / GPIO20 are the USB D- / D+ pins of the S3) */
    {3,  "GPIO3",  "None",         false},
    {19, "GPIO19", "None",         false},
    {20, "GPIO20", "None",         false},
    {40, "GPIO40", "None",         false},
    {41, "GPIO41", "None",         false},
    {42, "GPIO42", "None",         false},
    {46, "GPIO46", "None",         false},
};
#elif BOARD == BOARD_RS485_CAN
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
    {17, "GPIO17", "None",    false},
    {18, "GPIO18", "None",    false},
    {21, "GPIO21", "None",    false},
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
