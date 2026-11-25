#ifndef SERIAL_RAW_H
#define SERIAL_RAW_H

/**
 * @file serial_raw.h
 * @brief Raw serial protocol layer: plain bytes over a serial bus port.
 *
 * Layering:
 *
 *     UART hardware  ->  serial_bus  ->  serial_raw  ->  caller (TI script, native module)
 *
 * The caller never touches the serial bus directly: serial_raw registers on the bus with the fixed owner
 * name "serial_raw", opens the UART with the baud rate and frame format defined here, and moves the bytes.
 *
 * Only ONE caller can use a port in raw mode. Every function takes the port number (1..SERIAL_BUS_PORT_COUNT)
 * as the key, there is no instance or handle: serial_raw keeps its own per-port state and refuses a call
 * on a port that is not open.
 *
 * Results are returned as SerialRawStatus_t. SerialRaw_StatusToStr() gives the text used by logs and
 * returned to scripts ("OK", "Port not open", ...).
 */

/*---- INCLUDES ----*/
#include <Arduino.h>
#include "hardware/bus/serial_bus.h"

/*---- DEFINES ----*/

/** Owner name serial_raw registers on the serial bus with. */
#define SERIAL_RAW_OWNER_NAME   "serial_raw"

/** Idle time, in characters, after which the UART treats received bytes as one finished burst. */
#define SERIAL_RAW_RX_GAP_CHARS     2

/** RX FIFO threshold (bytes). High on purpose: bytes reach the RX buffer on the idle gap, not one by one. */
#define SERIAL_RAW_RX_FIFO_FULL     120

/** Largest number of bytes handed to an RX callback in one call. */
#define SERIAL_RAW_RX_CHUNK         256

/*---- TYPES ----*/

/**
 * @brief Line settings a port is opened with.
 * @details The frame is Arduino's own constant (SERIAL_8N1, SERIAL_8E1, SERIAL_7O2, ...), passed to
 *          HardwareSerial::begin() as is. A caller that only has a name (a script) gets the constant
 *          from SerialRaw_FrameFromStr().
 */
typedef struct {
    uint32_t baud;      /**< Bits per second, e.g. 9600. Must be greater than 0. */
    uint32_t frame;     /**< Arduino frame constant: SERIAL_<data bits><parity><stop bits>, e.g. SERIAL_8N1. */
} SerialRawConfig_t;

/**
 * @brief Called when received bytes are ready on a port.
 * @details Runs in the UART event task of the Arduino core (not in an ISR) once the line was idle, with every
 *          byte that arrived. Bursts longer than SERIAL_RAW_RX_CHUNK arrive as several calls. Keep it short and
 *          never block. data is only valid during the call: copy what you need.
 * @param port Port the bytes came from.
 * @param data Received bytes.
 * @param len  Number of bytes in data.
 */
typedef void (*SerialRawRxCb_t)(uint8_t port, const uint8_t *data, size_t len);

/** Result of the serial_raw functions. SERIAL_RAW_OK is 0, everything else is an error. */
typedef enum {
    SERIAL_RAW_OK = 0,              /**< Success. */
    SERIAL_RAW_ERR_INVALID_PORT,    /**< Port number outside 1..SERIAL_BUS_PORT_COUNT. */
    SERIAL_RAW_ERR_INVALID_ARG,     /**< NULL pointer, zero length or invalid configuration. */
    SERIAL_RAW_ERR_PORT_BUSY,       /**< Another owner holds the port on the serial bus. */
    SERIAL_RAW_ERR_ALREADY_OPEN,    /**< serial_raw already opened this port (only one caller is accepted). */
    SERIAL_RAW_ERR_NO_PINS,         /**< SERIAL<n>_TX / _RX not configured. */
    SERIAL_RAW_ERR_NOT_OPEN,        /**< Port is not open (never opened, closed, or begin() failed). */
    SERIAL_RAW_ERR_UART,            /**< The UART failed to start. */
} SerialRawStatus_t;

/*---- API ----*/

/**
 * @brief Convert a frame name such as "8N1" to Arduino's frame constant.
 * @details Accepts the usual modes: <7|8><N|E|O><1|2> (8N1, 8N2, 8E1, 8E2, 8O1, 8O2, 7N1, 7N2, 7E1, 7E2, 7O1, 7O2).
 *          Case sensitive.
 * @param[in]  name Frame name.
 * @param[out] out  Receives the constant (SERIAL_8N1, ...). Not written when the name is unknown.
 * @return True when the name is known.
 */
bool SerialRaw_FrameFromStr(const char *name, uint32_t *out);

/**
 * @brief Open a port in raw mode.
 * @param[in] port Port number, 1..SERIAL_BUS_PORT_COUNT.
 * @param[in] cfg  Baud rate and frame format.
 * @return SERIAL_RAW_OK or an error code.
 */
SerialRawStatus_t SerialRaw_Open(uint8_t port, const SerialRawConfig_t *cfg);

/**
 * @brief Close a port opened by SerialRaw_Open and give it back to the bus.
 * @param[in] port Port number.
 * @return SERIAL_RAW_OK or an error code.
 */
SerialRawStatus_t SerialRaw_Close(uint8_t port);

/**
 * @brief Send bytes. Toggles the DE pin when the port has one and returns when the last byte has left the UART.
 * @param[in] port Port number.
 * @param[in] data Bytes to send.
 * @param[in] len  Number of bytes.
 * @return SERIAL_RAW_OK or an error code.
 */
SerialRawStatus_t SerialRaw_Write(uint8_t port, const uint8_t *data, size_t len);

/**
 * @brief Number of received bytes waiting in the RX buffer.
 * @details Open() sets an RX idle timeout, so the UART only moves bytes into this buffer after the line was
 *          quiet for SERIAL_RAW_RX_GAP_CHARS characters (or 120 bytes arrived). A value above 0 therefore means
 *          at least one complete burst is ready for SerialRaw_Read().
 * @param[in] port Port number.
 * @return Byte count; 0 when the port is invalid, not open or nothing arrived.
 */
size_t SerialRaw_Available(uint8_t port);

/**
 * @brief Take the bytes waiting in the RX buffer. Never waits.
 * @details Copies min(SerialRaw_Available(), max_len) bytes. When more are waiting, the rest stays in the
 *          buffer for the next call. Nothing waiting is not an error: OK with *out_len = 0.
 * @param[in]  port    Port number.
 * @param[out] buf     Receives the bytes.
 * @param[in]  max_len Size of buf. 0 takes nothing (buf is not touched, it may be NULL).
 * @param[out] out_len Number of bytes stored in buf; always written, 0 on every error.
 * @return SERIAL_RAW_OK or an error code.
 */
SerialRawStatus_t SerialRaw_Read(uint8_t port, uint8_t *buf, size_t max_len, size_t *out_len);

/**
 * @brief Register the function called when bytes are received on a port.
 * @details While a callback is set, the received bytes are handed to it and are NOT left in the RX buffer
 *          (SerialRaw_Available() / SerialRaw_Read() see nothing). Closing the port removes the callback.
 * @param[in] port Port number.
 * @param[in] cb   Function to call, or NULL to remove it.
 * @return SERIAL_RAW_OK or an error code.
 */
SerialRawStatus_t SerialRaw_SetRxCallback(uint8_t port, SerialRawRxCb_t cb);

/**
 * @brief Send a request and read the answer as one uninterruptible operation.
 * @param[in]  port       Port number.
 * @param[in]  tx         Request bytes.
 * @param[in]  tx_len     Request length.
 * @param[out] rx         Receives the answer.
 * @param[in]  rx_max     Size of rx.
 * @param[out] rx_len     Number of bytes stored in rx.
 * @param[in]  timeout_ms How long to wait for the first byte of the answer.
 * @return SERIAL_RAW_OK or an error code.
 */
SerialRawStatus_t SerialRaw_Transact(uint8_t port, const uint8_t *tx, size_t tx_len,
                                     uint8_t *rx, size_t rx_max, size_t *rx_len, uint32_t timeout_ms);

/**
 * @brief Readable text for a status code (logs, and the string returned to scripts).
 * @param[in] status Status code.
 * @return Constant string, never NULL.
 */
const char *SerialRaw_StatusToStr(SerialRawStatus_t status);

#endif // SERIAL_RAW_H
