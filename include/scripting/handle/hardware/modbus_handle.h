#ifndef MODBUS_HANDLE_H
#define MODBUS_HANDLE_H

/**
 * @file modbus_handle.h
 * @brief Script handle "modbus": one Modbus RTU master on a port used by a TI script, built on xdrv_04_serial_modbus.
 *
 * Layering:
 *
 *     xdrv_04_serial_modbus  ->  modbus handle  ->  TI script (hardware.MODBUS())
 *
 * The handle owns the port: the port is opened in Modbus mode by the script through the handle and closed
 * when the last value pointing to the handle is destroyed (end of script included). There is no other owner.
 * xdrv_04_serial_modbus builds the RTU frame (address, PDU, CRC) and handles the 3.5 character silence.
 *
 * request() runs in modbus_task, a task created for each request, not in the script: the script waits with
 * ti_wait_flag, so its signal handlers and events keep running. One request at a time per handle. The native
 * returns only after modbus_task has left the handle (also when the script is stopped), so the destructor
 * never frees a handle that modbus_task is still using.
 *
 * Script view:
 *   attributes: port, baud, frame (read only)
 *   methods:    open(port, baud, frame)                              -> "OK" or the error text of xdrv_04_serial_modbus
 *               request(slave, function, address, quantity, timeout_ms) -> {"status": text, "data": bytes}
 *               close()                                              -> "OK" or the error text of xdrv_04_serial_modbus
 *   Calling request / close on a modbus that is not open raises an error.
 *   request only accepts the read functions 1..4. A write function (5, 6, 15, 16, ...) or any other code raises
 *   an error and is never sent: scripts must not be able to change a machine through Modbus.
 */

/*---- INCLUDES ----*/
#include <Arduino.h>
#include "TienInterpreter.h"
#include "hardware/driver/xdrv_04_serial_modbus.h"

/*---- TYPES ----*/

/**
 * @brief Native object wrapped by a "modbus" handle value.
 */
typedef struct {
    uint8_t              port;      /**< Port number 1..SERIAL_MODBUS_PORT_MAX, 0 before open(). */
    uint32_t             baud;      /**< Baud rate given to open(). */
    ModbusSerialConfig_t frame;     /**< RTU frame given to open() (RTU_8N1, RTU_8E1, ...). */
    bool                 is_open;   /**< True while this handle holds the port in xdrv_04_serial_modbus. */
    bool                 busy;      /**< Set while a request runs, cleared by the native once it read the result. */

    /* Request: written by the native before it starts modbus_task */
    uint8_t              slave;       /**< Slave address. */
    uint8_t              function;    /**< Function code (reads 1..4 only). */
    uint16_t             address;     /**< First register / coil. */
    uint16_t             quantity;    /**< Number of registers / coils. */
    uint32_t             timeout_ms;  /**< Answer timeout. */
    ti_handle_t          runtime;     /**< Script that made the request: woken when it ended. */

    /* Result: written by modbus_task */
    bool                 done;                              /**< Set when the request ended, waited with ti_wait_flag. */
    SerialModbusStatus_t status;                            /**< Result of SerialModbus_Send. */
    uint8_t              data[SERIAL_MODBUS_MAX_RESPONSE];  /**< Answer: 2 bytes per register, high byte first. */
    size_t               data_len;                          /**< Bytes in data (0 on error). */
} modbus_handle_t;

/*---- HANDLE TYPE ----*/

/**
 * @brief Handle type of a modbus value: name "modbus", destructor, methods and attributes.
 * @details Wrap a calloc'ed modbus_handle_t with val_new_handle(ptr, &g_modbus_handle_type). The value then owns
 *          it: the destructor closes the port if still open and frees it when the last value is freed.
 *          Only defined when USE_MODBUS_MASTER is set.
 */
extern const ti_handle_type_t g_modbus_handle_type;

#endif // MODBUS_HANDLE_H
