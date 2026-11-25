#ifndef SERIAL_BUS_H
#define SERIAL_BUS_H

/**
 * @file serial_bus.h
 * @brief Serial bus: hardware arbiter for the board's logical serial ports.
 *
 * Layering (bottom to top):
 *
 *     UART hardware  ->  serial_bus  ->  protocol layer (Modbus / raw)  ->  device
 *
 * The bus knows NOTHING about protocols. It only manages hardware resources:
 *   - the GPIOs of a port (pin configuration names SERIAL<n>_TX / _RX / _DE),
 *   - the hardware UART, fixed per port: port 1 = UART1 = Serial1, port 2 = UART2 = Serial2 (UART0 is the console and is never handed out),
 *   - the OWNERSHIP of the port, identified by an owner name chosen by the protocol layer.
 *
 * Baud rate, frame format and everything else about the protocol belong to the module that
 * acquired the port. The bus never stores or checks them.
 *
 * A "port" is a number 1..SERIAL_BUS_PORT_COUNT and is the UART number: port n = UART n. There is no allocator.
 *
 * Pin configuration names are built from the port number: SERIAL<n>_TX, SERIAL<n>_RX (required) and
 * SERIAL<n>_DE (optional, driver-enable of an RS485 transceiver; leave it unset for a plain TTL / RS232 port).
 * The bus only checks that n <= SERIAL_BUS_PORT_COUNT. TX and RX must be different GPIOs. The pins are resolved
 * by the first acquire and kept until the port is released, so a pin configuration change only takes effect
 * after the port is released and acquired again.
 */

/*---- INCLUDES ----*/
#include <Arduino.h>
#include <HardwareSerial.h>
#include <soc/soc_caps.h>       /* SOC_UART_NUM */

/*---- DEFINES ----*/

/** Number of ports the chip supports: every UART except UART0 (console). 2 on ESP32 / ESP32-S3, 1 on ESP32-C3. */
#define SERIAL_BUS_PORT_COUNT       (SOC_UART_NUM - 1)

/** Longest owner name, without the terminator. Longer names are rejected. */
#define SERIAL_BUS_OWNER_NAME_MAX   15

/*---- TYPES ----*/

/**
 * @brief Resources of an acquired port: the UART object and the GPIOs.
 * @details Filled by the bus for every successful acquire (the first owner and the ones that
 *          join). The owner calls begin() itself, e.g.
 *          res.serial->begin(baud, config, res.rx_pin, res.tx_pin);
 *          Not to be confused with Arduino's HardwareSerial: this struct only CONTAINS a pointer to one.
 */
typedef struct {
    HardwareSerial *serial;     /**< UART object: &Serial1 or &Serial2 */
    int8_t          tx_pin;     /**< UART transmit GPIO */
    int8_t          rx_pin;     /**< UART receive GPIO */
    int8_t          de_pin;     /**< Driver-enable GPIO of an RS485 transceiver, GPIO_INVALID (-1) if the port has none */
} SerialResources_t;

/**
 * @brief Result of the bus functions. SERIAL_BUS_OK is 0.
 * @note SERIAL_BUS_ALREADY_ACQUIRED is NOT an error: the port is ready and shared with the same owner.
 *       Every other value other than SERIAL_BUS_OK is an error.
 */
typedef enum {
    SERIAL_BUS_OK = 0,              /**< First owner: the port is now open. The owner must initialise its protocol. */
    SERIAL_BUS_ALREADY_ACQUIRED,    /**< Same owner already holds the port: just use it, do not initialise again. */
    SERIAL_BUS_ERR_CONFLICT,        /**< Another owner holds the port. */
    SERIAL_BUS_ERR_INVALID_PORT,    /**< Port number outside 1..SERIAL_BUS_PORT_COUNT. */
    SERIAL_BUS_ERR_INVALID_ARG,     /**< NULL pointer, empty owner name or owner name longer than SERIAL_BUS_OWNER_NAME_MAX. */
    SERIAL_BUS_ERR_NO_PINS,         /**< SERIAL<n>_TX / _RX not assigned, or the name did not resolve to a GPIO. */
    SERIAL_BUS_ERR_NOT_OWNER,       /**< Release by a caller that does not hold the port. */
} SerialBusStatus_t;

/*---- LIFECYCLE ----*/

/**
 * @brief Claim a port for an owner, or join a port the same owner already holds.
 *
 * @details Result and what the caller must do:
 *          - SERIAL_BUS_OK:               first owner. *out_res points to the port's resources and the caller
 *                                         initialises its protocol on them (the bus never calls begin(): the
 *                                         caller does, on (*out_res)->serial).
 *          - SERIAL_BUS_ALREADY_ACQUIRED: the same owner already holds the port. *out_res points to the resources,
 *                                         the port is already initialised: share it, do not initialise again.
 *          - SERIAL_BUS_ERR_CONFLICT:     another owner holds the port.
 *          - other SERIAL_BUS_ERR_*:      nothing changed, *out_res is NOT written.
 *
 *          The caller may set its pointer to NULL before the call and test it afterwards. The pointed-to
 *          structure belongs to the bus (no copy): it is read-only and valid until the matching release.
 *
 *          Every successful acquire (OK or ALREADY_ACQUIRED) must be matched by exactly one release.
 *          Thread safe. The bus does not check baud rate or frame format: that is the protocol layer's job.
 *
 * @param[in]  port       Port number, 1..SERIAL_BUS_PORT_COUNT.
 * @param[in]  owner_name Name of the owner, 1..SERIAL_BUS_OWNER_NAME_MAX characters, copied by the bus.
 * @param[out] out_res    Address of the caller's pointer, set to the resources of the port on success.
 * @return SERIAL_BUS_OK, SERIAL_BUS_ALREADY_ACQUIRED or an error code.
 */
SerialBusStatus_t SerialBus_Acquire(uint8_t port, const char *owner_name, const SerialResources_t **out_res);

/**
 * @brief Give back one acquire of a port.
 *
 * @details Result and what the caller must do:
 *          - SERIAL_BUS_OK:               last release: the port is free again. The caller tears its protocol down
 *                                         (the bus never calls end()).
 *          - SERIAL_BUS_ALREADY_ACQUIRED: other acquires of the same owner still use the port: do NOT tear down.
 *          - SERIAL_BUS_ERR_NOT_OWNER:    the caller does not hold the port. Nothing changed.
 *          - other SERIAL_BUS_ERR_*:      invalid argument. Nothing changed.
 *
 *          Thread safe: the bus state is changed under the bus lock, so a concurrent acquire waits for it.
 *
 * @param[in] port       Port number, 1..SERIAL_BUS_PORT_COUNT.
 * @param[in] owner_name Name the port was acquired with.
 * @return SERIAL_BUS_OK, SERIAL_BUS_ALREADY_ACQUIRED or an error code.
 */
SerialBusStatus_t SerialBus_Release(uint8_t port, const char *owner_name);

/*---- QUERY ----*/

/**
 * @brief Name of the owner currently holding a port (for logs and error messages).
 * @details The returned string lives inside the bus: use it right away and do not keep it, it stops being
 *          valid when the port is released.
 * @param[in] port Port number, 1..SERIAL_BUS_PORT_COUNT.
 * @return Owner name, or NULL when the port is free or the port number is invalid.
 */
const char *SerialBus_GetOwner(uint8_t port);

/**
 * @brief Readable text for a status code (for logs and script error messages).
 * @param[in] status Status code.
 * @return Constant string, never NULL.
 */
const char *SerialBus_StatusToStr(SerialBusStatus_t status);

#endif // SERIAL_BUS_H
