#ifndef UART_HANDLE_H
#define UART_HANDLE_H

/**
 * @file uart_handle.h
 * @brief Script handle "uart": one UART port used by a TI script, built on top of serial_raw.
 *
 * Layering:
 *
 *     serial_raw  ->  uart handle  ->  TI script (hardware.UART())
 *
 * The handle owns the port: the port is opened by the script through the handle and closed when the
 * last value pointing to the handle is destroyed (end of script included). There is no other owner.
 *
 * Script view:
 *   attributes: port, baud, frame
 *   methods:    open(port, baud, frame) -> "OK" or the error text of serial_raw
 *               write(data)             -> "OK" or the error text of serial_raw (data: string or bytes)
 *               is_available()          -> bool, true when received bytes are waiting
 *               read(max_len)           -> bytes, up to max_len received bytes (empty when nothing waits)
 *               on_receive(handler)     -> "OK" or the error text of serial_raw; handler(bytes data) runs per
 *                                          received burst. While set, is_available() / read() see nothing.
 *   Calling write / is_available / read / on_receive on a uart that is not open raises an error.
 */

/*---- INCLUDES ----*/
#include <Arduino.h>
#include "TienInterpreter.h"
#include "hardware/protocol/serial_raw.h"

/*---- TYPES ----*/

/**
 * @brief Native object wrapped by a "uart" handle value.
 */
typedef struct {
    uint8_t           port;         /**< Port number 1..SERIAL_BUS_PORT_COUNT, 0 before open(). */
    SerialRawConfig_t cfg;          /**< Baud rate and Arduino frame constant given to open(). */
    bool              is_open;      /**< True while this handle holds the port in serial_raw. */
} uart_handle_t;

/*---- HANDLE TYPE ----*/

/**
 * @brief Handle type of a uart value: name "uart", destructor, methods and attributes.
 * @details Wrap a calloc'ed uart_handle_t with val_new_handle(ptr, &g_uart_handle_type). The value then owns
 *          it: the destructor closes the port if still open and frees it when the last value is freed.
 */
extern const ti_handle_type_t g_uart_handle_type;

#endif // UART_HANDLE_H
