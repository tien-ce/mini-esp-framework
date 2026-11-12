#ifndef MODBUS_HANDLE_H
#define MODBUS_HANDLE_H

/**
 * @file modbus_handle.h
 * @brief Script handle "modbus": one Modbus RTU master on a UART port used by a TI script, built on top of modbus_master.
 *
 * Layering:
 *
 *     modbus_master  ->  modbus handle  ->  TI script (hardware.MODBUS())
 *
 * The handle owns the port: the port is opened in Modbus mode by the script through the handle and closed
 * when the last value pointing to the handle is destroyed (end of script included). There is no other owner.
 * modbus_master builds the RTU frame (address, PDU, CRC) and handles the 3.5 character silence.
 *
 * Script view:
 *   attributes: port, baud, frame (read only)
 *   methods:    open(port, baud, frame)                              -> "OK" or the error text of modbus_master
 *               request(slave, function, address, quantity, timeout_ms) -> {"status": text, "data": bytes}
 *               close()                                              -> "OK" or the error text of modbus_master
 *   Calling request / close on a modbus that is not open raises an error.
 */

/*---- INCLUDES ----*/
#include <Arduino.h>
#include "TienInterpreter.h"
#include "hardware/protocol/modbus_master.h"

/*---- TYPES ----*/

/**
 * @brief Native object wrapped by a "modbus" handle value.
 */
typedef struct {
    uint8_t              port;      /**< Port number 1..SERIAL_BUS_PORT_COUNT, 0 before open(). */
    uint32_t             baud;      /**< Baud rate given to open(). */
    ModbusSerialConfig_t frame;     /**< RTU frame given to open() (RTU_8N1, RTU_8E1, ...). */
    bool                 is_open;   /**< True while this handle holds the port in modbus_master. */
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
