#ifndef XDRV_03_SERIAL_UART_H
#define XDRV_03_SERIAL_UART_H

/**
 * @file xdrv_03_serial_uart.h
 * @brief Serial UART driver: plain bytes on a logical UART port.
 *
 * Layering:
 *
 *     xdrv_03_serial_uart  ->  uart handle  ->  TI script (hardware.UART())
 *
 * Port n is logical: its pins are the pin options SERIAL_UART<n>_TX, SERIAL_UART<n>_RX (required) and
 * SERIAL_UART<n>_DE (optional: driver enable of an RS485 transceiver), read once at SIG_INIT. Open() takes any
 * free hardware UART (Serial1, Serial2) under the UART lock of driver_shared, Close() stops it so another driver
 * can take it. Only one user per port: every function takes the port number, there is no handle.
 */

/*---- INCLUDES ----*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <soc/soc_caps.h>       /* SOC_UART_NUM */

/*---- DEFINES ----*/

/** Number of logical UART ports: as many as the hardware UARTs given out (UART0 is the console). */
#define SERIAL_UART_PORT_MAX        (SOC_UART_NUM - 1)

/** Idle time, in characters, after which the UART treats received bytes as one finished burst. */
#define SERIAL_UART_RX_GAP_CHARS    2

/** RX FIFO threshold (bytes). High on purpose: bytes reach the RX buffer on the idle gap, not one by one. */
#define SERIAL_UART_RX_FIFO_FULL    120

/** Largest number of bytes handed to an RX callback in one call. */
#define SERIAL_UART_RX_CHUNK        256

/*---- TYPES ----*/

/** Line settings a port is opened with. */
typedef struct {
    uint32_t baud;      /**< Bits per second, e.g. 9600. Must be greater than 0. */
    uint32_t frame;     /**< Arduino frame constant: SERIAL_<data bits><parity><stop bits>, e.g. SERIAL_8N1. */
} SerialUartConfig_t;

/**
 * @brief Called when received bytes are ready on a port.
 * @details Runs in the UART event task of the Arduino core (not in an ISR) once the line was idle. Bursts longer
 *          than SERIAL_UART_RX_CHUNK arrive as several calls. Keep it short and never block. data is only valid
 *          during the call. It runs with the port lock held, so SerialUart_Close() waits for it and no call
 *          happens after Close() returned. Never call a SerialUart_ function from it (deadlock).
 * @param port Port the bytes came from.
 * @param data Received bytes.
 * @param len  Number of bytes in data.
 * @param arg  The arg given to SerialUart_SetRxCallback() with this callback.
 */
typedef void (*SerialUartRxCallback_t)(uint8_t port, const uint8_t *data, size_t len, void *arg);

/** Result of the serial UART functions. SERIAL_UART_OK is 0, everything else is an error. */
typedef enum {
    SERIAL_UART_OK = 0,             /**< Success. */
    SERIAL_UART_ERR_NOT_INIT,       /**< The driver has not run SIG_INIT yet. */
    SERIAL_UART_ERR_OUT_OF_RANGE,   /**< Port number outside 1..SERIAL_UART_PORT_MAX. */
    SERIAL_UART_ERR_INVALID_ARG,    /**< NULL pointer, zero length or baud 0. */
    SERIAL_UART_ERR_NO_PIN,         /**< SERIAL_UART<n>_TX / _RX not set, or both on the same GPIO. */
    SERIAL_UART_ERR_ALREADY_OPEN,   /**< The port is already open. */
    SERIAL_UART_ERR_NOT_OPEN,       /**< The port is not open. */
    SERIAL_UART_ERR_NO_FREE_UART,   /**< Every hardware UART is used (by this driver or another one). */
    SERIAL_UART_ERR_UART,           /**< The UART failed to start, or did not take every byte to send. */
} SerialUartStatus_t;

/*---- API ----*/

/**
 * @brief Open a port: take a free hardware UART and start it on the port's pins.
 * @param[in] port Port number, 1..SERIAL_UART_PORT_MAX.
 * @param[in] cfg  Baud rate and frame format.
 * @return SERIAL_UART_OK or an error code.
 */
SerialUartStatus_t SerialUart_Open(uint8_t port, const SerialUartConfig_t *cfg);

/**
 * @brief Close a port: remove its RX callback and stop its UART (free for another driver after this).
 * @param[in] port Port number.
 * @return SERIAL_UART_OK or an error code.
 */
SerialUartStatus_t SerialUart_Close(uint8_t port);

/**
 * @brief Send bytes. Drives DE high while sending when the port has one, returns when the last bit has left.
 * @param[in] port Port number.
 * @param[in] data Bytes to send.
 * @param[in] len  Number of bytes, greater than 0.
 * @return SERIAL_UART_OK or an error code.
 */
SerialUartStatus_t SerialUart_Write(uint8_t port, const uint8_t *data, size_t len);

/**
 * @brief Number of received bytes waiting in the RX buffer (0 when the port is not open).
 * @details Bytes reach the buffer after SERIAL_UART_RX_GAP_CHARS characters of silence (or
 *          SERIAL_UART_RX_FIFO_FULL bytes), so a value above 0 means a finished burst. Always 0 while an RX
 *          callback is set: the callback takes the bytes.
 * @param[in] port Port number.
 * @return Number of bytes waiting.
 */
size_t SerialUart_Available(uint8_t port);

/**
 * @brief Take the bytes waiting in the RX buffer. Never waits.
 * @param[in]  port    Port number.
 * @param[out] buf     Receives the bytes.
 * @param[in]  max_len Size of buf. 0 takes nothing (buf may then be NULL).
 * @param[out] out_len Number of bytes stored in buf; always written, 0 on every error.
 * @return SERIAL_UART_OK or an error code.
 */
SerialUartStatus_t SerialUart_Read(uint8_t port, uint8_t *buf, size_t max_len, size_t *out_len);

/**
 * @brief Set or remove the function called with the received bytes of an open port.
 * @details While set, received bytes go to it and not to the RX buffer. Closing the port removes it.
 * @param[in] port     Port number.
 * @param[in] callback Function to call, or NULL to remove it.
 * @param[in] arg      Passed as is to every call of callback.
 * @return SERIAL_UART_OK or an error code.
 */
SerialUartStatus_t SerialUart_SetRxCallback(uint8_t port, SerialUartRxCallback_t callback, void *arg);

/**
 * @brief Readable text for a status code (logs, and the string returned to scripts).
 * @param[in] status Status code.
 * @return Constant string, never NULL.
 */
const char *SerialUart_StatusToStr(SerialUartStatus_t status);

#endif // XDRV_03_SERIAL_UART_H
