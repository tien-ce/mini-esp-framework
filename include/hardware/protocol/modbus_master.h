#ifndef MODBUS_MASTER_H
#define MODBUS_MASTER_H

/**
 * @file modbus_master.h
 * @brief Modbus master protocol layer: one Modbus RTU instance per serial bus port.
 *
 * Layering:
 *
 *     UART hardware  ->  serial_bus  ->  modbus_master  ->  caller (TI script, adapter)
 *
 * modbus_master registers on the bus with the fixed owner name "modbus", gives the pins to the Modbus library
 * and keeps one library instance per port. Every function takes the port number (1..SERIAL_BUS_PORT_COUNT)
 * as the key.
 *
 * A port is opened ONCE (by a script or by the application) and then used by any number of devices (slaves)
 * through the transaction functions; adapters never open a port, they only check ModbusMaster_IsOpen().
 * Opening an open port is refused, there is no reference counting and no configuration check.
 *
 * Results are returned as ModbusMasterStatus_t. ModbusMaster_StatusToStr() gives the text used by logs and
 * returned to scripts ("OK", "Port not open", ...).
 */

/*---- INCLUDES ----*/
#include <Arduino.h>
#include "port.h"
#include "hardware/bus/serial_bus.h"

/*---- DEFINES ----*/

/** Owner name modbus_master registers on the serial bus with. */
#define MODBUS_MASTER_OWNER_NAME   "modbus"

/** Size of the answer buffer that always fits: 125 registers (MAX_VALUE_RESPONE of the library) x 2 bytes. */
#define MODBUS_MASTER_MAX_RESPONSE 250

/*---- TYPES ----*/

/** Result of the modbus_master functions. MBM_OK is 0, everything else is an error. */
typedef enum {
    MBM_OK = 0,                 /**< Success. */
    MBM_ERR_INVALID_PORT,       /**< Port number outside 1..SERIAL_BUS_PORT_COUNT. */
    MBM_ERR_INVALID_ARG,        /**< Invalid argument (e.g. baud rate 0). */
    MBM_ERR_PORT_BUSY,          /**< Another owner holds the port on the serial bus. */
    MBM_ERR_ALREADY_OPEN,       /**< The port is already open in Modbus mode. */
    MBM_ERR_NO_PINS,            /**< SERIAL<n>_TX / _RX not configured. */
    MBM_ERR_NOT_OPEN,           /**< The port is not open. */
    MBM_ERR_UART,               /**< The UART or the Modbus stack failed to start. */
    MBM_ERR_REQUEST,            /**< The library could not build the request (e.g. unsupported function). */
    MBM_ERR_TIMEOUT,            /**< No answer from the slave before the timeout. */
    MBM_ERR_RESPONSE,           /**< An answer arrived but it is invalid (CRC, wrong slave, etc.). */
    MBM_ERR_BUFFER_TOO_SMALL,   /**< The caller's buffer cannot hold the answer; nothing was copied. */
    /* Exeception respone from Slave */
    MBM_ERR_EXCEP_ILLEGAL_FUNCTION,     /**< Slave exception 0x01: Illegal function. */
    MBM_ERR_EXCEP_ILLEGAL_DATA_ADDR,    /**< Slave exception 0x02: Illegal data address. */
    MBM_ERR_EXCEP_ILLEGAL_DATA_VAL,     /**< Slave exception 0x03: Illegal data value. */
    MBM_ERR_EXCEP_SLAVE_DEVICE_FAIL,    /**< Slave exception 0x04: Slave device failure. */
    MBM_ERR_EXCEP_ACKNOWLEDGE,          /**< Slave exception 0x05: Acknowledge. */
    MBM_ERR_EXCEP_SLAVE_DEVICE_BUSY,    /**< Slave exception 0x06: Slave device busy. */
    MBM_ERR_EXCEP_MEMORY_PARITY_ERROR,  /**< Slave exception 0x08: Memory parity error. */
    MBM_ERR_EXCEP_GATEWAY_PATH_UNAVAIL, /**< Slave exception 0x0A: Gateway path unavailable. */
    MBM_ERR_EXCEP_GATEWAY_TARGET_FAIL,  /**< Slave exception 0x0B: Gateway target device failed to respond. */
    MBM_ERR_EXCEP_UNKNOWN,              /**< Slave exception: Unknown/unrecognized exception code. */
} ModbusMasterStatus_t;

/*---- API ----*/

/**
 * @brief Convert a frame name such as "8N1" to the library's frame constant.
 * @details Accepts the usual modes: <7|8><N|E|O><1|2> (8N1, 8N2, 8E1, 8E2, 8O1, 8O2, 7N1, 7N2, 7E1, 7E2, 7O1, 7O2).
 *          Case sensitive.
 * @param[in]  name Frame name.
 * @param[out] out  Receives the constant (RTU_8N1, ...). Not written when the name is unknown.
 * @return True when the name is known.
 */
bool ModbusMaster_FrameFromStr(const char *name, ModbusSerialConfig_t *out);

/**
 * @brief Open a port in Modbus RTU master mode.
 * @param[in] port  Port number, 1..SERIAL_BUS_PORT_COUNT.
 * @param[in] baud  Bits per second, must be greater than 0.
 * @param[in] frame Line format: data bits, parity, stop bits (RTU_8N1, RTU_8E1, ...).
 * @return MBM_OK or an error code.
 */
ModbusMasterStatus_t ModbusMaster_Open(uint8_t port, uint32_t baud, ModbusSerialConfig_t frame);

/**
 * @brief Close a port opened by ModbusMaster_Open and give it back to the bus.
 * @details Closes it for every user of the port.
 * @param[in] port Port number.
 * @return MBM_OK or an error code.
 */
ModbusMasterStatus_t ModbusMaster_Close(uint8_t port);

/**
 * @brief Tell whether a port is open and ready for Modbus transactions.
 * @param[in] port Port number.
 * @return True when open; false when closed or the port number is invalid.
 */
bool ModbusMaster_IsOpen(uint8_t port);

/**
 * @brief Send one request to a slave and wait for the answer.
 * @details Works for every function the library can build (read 1..4, write single coil / register 5, 6).
 *          For a write, quantity carries the value to write. The library serialises the transactions of a port.
 *          Nothing is checked here except the port: slave, function and quantity are checked by the library
 *          (MBM_ERR_REQUEST).
 * @param[in]     port       Port number.
 * @param[in]     slave      Slave address.
 * @param[in]     function   Modbus function code.
 * @param[in]     address    Start address (or the single coil / register for a write).
 * @param[in]     quantity   Number of items to read, or the value to write.
 * @param[in]     timeout_ms How long to wait for the answer.
 * @param[out]    buf        Receives the answer data, 2 bytes per register, high byte first.
 * @param[in,out] len        In: size of buf (MODBUS_MASTER_MAX_RESPONSE always fits). Out: bytes stored, 0 on error.
 * @return MBM_OK or an error code.
 */
ModbusMasterStatus_t ModbusMaster_Send(uint8_t port, uint8_t slave, uint8_t function,
                                       uint16_t address, uint16_t quantity, uint32_t timeout_ms,
                                       uint8_t *buf, size_t *len);

/**
 * @brief Readable text for a status code (logs, and the string returned to scripts).
 * @param[in] status Status code.
 * @return Constant string, never NULL.
 */
const char *ModbusMaster_StatusToStr(ModbusMasterStatus_t status);

#endif // MODBUS_MASTER_H
