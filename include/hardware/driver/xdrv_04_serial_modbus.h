#ifndef XDRV_04_SERIAL_MODBUS_H
#define XDRV_04_SERIAL_MODBUS_H

/**
 * @file xdrv_04_serial_modbus.h
 * @brief Serial Modbus driver: one Modbus RTU master on a logical Modbus port.
 *
 * Layering:
 *
 *     xdrv_04_serial_modbus  ->  modbus handle  ->  TI script (hardware.MODBUS())
 *
 * Port n is logical: its pins are the pin options SERIAL_MODBUS<n>_TX, SERIAL_MODBUS<n>_RX (required) and
 * SERIAL_MODBUS<n>_DE (optional: driver enable of an RS485 transceiver), read once at SIG_INIT. Open() takes any
 * free hardware UART (Serial1, Serial2) under the UART lock of driver_shared and hands it to the Modbus library,
 * which starts it (begin) and drives DE itself. Close() stops it so another driver can take it.
 * The library builds the RTU frame (address, PDU, CRC) and handles the 3.5 character silence.
 * Every function takes the port number, there is no handle.
 */

/*---- INCLUDES ----*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <soc/soc_caps.h>       /* SOC_UART_NUM */
#include "port.h"               /* ModbusSerialConfig_t, RTU_8N1 ... (Modbus library) */

/*---- DEFINES ----*/

/** Number of logical Modbus ports: as many as the hardware UARTs given out (UART0 is the console). */
#define SERIAL_MODBUS_PORT_MAX      (SOC_UART_NUM - 1)

/** Size of the answer buffer that always fits: 125 registers (MAX_VALUE_RESPONE of the library) x 2 bytes. */
#define SERIAL_MODBUS_MAX_RESPONSE  250

/*---- TYPES ----*/

/** Line settings a port is opened with. */
typedef struct {
    uint32_t             baud;      /**< Bits per second, e.g. 9600. Must be greater than 0. */
    ModbusSerialConfig_t frame;     /**< Data bits, parity, stop bits: RTU_8N1, RTU_8E1, ... */
} SerialModbusConfig_t;

/** Result of the serial Modbus functions. SERIAL_MODBUS_OK is 0, everything else is an error. */
typedef enum {
    SERIAL_MODBUS_OK = 0,                   /**< Success. */
    SERIAL_MODBUS_ERR_NOT_INIT,             /**< The driver has not run SIG_INIT yet. */
    SERIAL_MODBUS_ERR_OUT_OF_RANGE,         /**< Port number outside 1..SERIAL_MODBUS_PORT_MAX. */
    SERIAL_MODBUS_ERR_INVALID_ARG,          /**< NULL pointer or baud 0. */
    SERIAL_MODBUS_ERR_NO_PIN,               /**< SERIAL_MODBUS<n>_TX / _RX not set, or both on the same GPIO. */
    SERIAL_MODBUS_ERR_ALREADY_OPEN,         /**< The port is already open. */
    SERIAL_MODBUS_ERR_NOT_OPEN,             /**< The port is not open. */
    SERIAL_MODBUS_ERR_NO_FREE_UART,         /**< Every hardware UART is used (by this driver or another one). */
    SERIAL_MODBUS_ERR_UART,                 /**< The UART or the Modbus stack failed to start. */
    SERIAL_MODBUS_ERR_REQUEST,              /**< The library could not build the request (e.g. unsupported function). */
    SERIAL_MODBUS_ERR_TIMEOUT,              /**< No answer from the slave before the timeout. */
    SERIAL_MODBUS_ERR_RESPONSE,             /**< An answer arrived but it is invalid (CRC, wrong slave, etc.). */
    SERIAL_MODBUS_ERR_BUFFER_TOO_SMALL,     /**< The caller's buffer cannot hold the answer; nothing was copied. */
    /* Exception response from the slave */
    SERIAL_MODBUS_ERR_EXCEP_ILLEGAL_FUNCTION,       /**< Slave exception 0x01: Illegal function. */
    SERIAL_MODBUS_ERR_EXCEP_ILLEGAL_DATA_ADDR,      /**< Slave exception 0x02: Illegal data address. */
    SERIAL_MODBUS_ERR_EXCEP_ILLEGAL_DATA_VAL,       /**< Slave exception 0x03: Illegal data value. */
    SERIAL_MODBUS_ERR_EXCEP_SLAVE_DEVICE_FAIL,      /**< Slave exception 0x04: Slave device failure. */
    SERIAL_MODBUS_ERR_EXCEP_ACKNOWLEDGE,            /**< Slave exception 0x05: Acknowledge. */
    SERIAL_MODBUS_ERR_EXCEP_SLAVE_DEVICE_BUSY,      /**< Slave exception 0x06: Slave device busy. */
    SERIAL_MODBUS_ERR_EXCEP_MEMORY_PARITY_ERROR,    /**< Slave exception 0x08: Memory parity error. */
    SERIAL_MODBUS_ERR_EXCEP_GATEWAY_PATH_UNAVAIL,   /**< Slave exception 0x0A: Gateway path unavailable. */
    SERIAL_MODBUS_ERR_EXCEP_GATEWAY_TARGET_FAIL,    /**< Slave exception 0x0B: Gateway target device failed to respond. */
    SERIAL_MODBUS_ERR_EXCEP_UNKNOWN,                /**< Slave exception: unknown exception code. */
} SerialModbusStatus_t;

/*---- API ----*/

/**
 * @brief Open a port: take a free hardware UART and start the Modbus RTU master on the port's pins.
 * @param[in] port Port number, 1..SERIAL_MODBUS_PORT_MAX.
 * @param[in] cfg  Baud rate and frame format.
 * @return SERIAL_MODBUS_OK or an error code.
 */
SerialModbusStatus_t SerialModbus_Open(uint8_t port, const SerialModbusConfig_t *cfg);

/**
 * @brief Close a port: stop the Modbus stack and its UART (free for another driver after this).
 * @details Waits for a running SerialModbus_Send() on the port to end.
 * @param[in] port Port number.
 * @return SERIAL_MODBUS_OK or an error code.
 */
SerialModbusStatus_t SerialModbus_Close(uint8_t port);

/**
 * @brief Send one request to a slave and wait for the answer.
 * @details Works for every function the library can build (read 1..4, write single coil / register 5, 6).
 *          For a write, quantity carries the value to write. The transactions of a port run one at a time.
 *          Slave, function and quantity are checked by the library (SERIAL_MODBUS_ERR_REQUEST).
 * @param[in]     port       Port number.
 * @param[in]     slave      Slave address.
 * @param[in]     function   Modbus function code.
 * @param[in]     address    Start address (or the single coil / register for a write).
 * @param[in]     quantity   Number of items to read, or the value to write.
 * @param[in]     timeout_ms How long to wait for the answer.
 * @param[out]    buf        Receives the answer data, 2 bytes per register, high byte first.
 * @param[in,out] len        In: size of buf (SERIAL_MODBUS_MAX_RESPONSE always fits). Out: bytes stored, 0 on error.
 * @return SERIAL_MODBUS_OK or an error code.
 */
SerialModbusStatus_t SerialModbus_Send(uint8_t port, uint8_t slave, uint8_t function,
                                       uint16_t address, uint16_t quantity, uint32_t timeout_ms,
                                       uint8_t *buf, size_t *len);

/**
 * @brief Readable text for a status code (logs, and the string returned to scripts).
 * @param[in] status Status code.
 * @return Constant string, never NULL.
 */
const char *SerialModbus_StatusToStr(SerialModbusStatus_t status);

#endif // XDRV_04_SERIAL_MODBUS_H
