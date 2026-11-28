/**
 * @file xdrv_03_serial_uart.cpp
 * @brief Serial UART driver: pins and locks prepared at SIG_INIT, a free hardware UART taken at open (see
 *        xdrv_03_serial_uart.h). No CLI command: scripts use it through the uart handle.
 */

/*---- INCLUDES ----*/
#include <Arduino.h>
#include <HardwareSerial.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "core/core_engine.h"
#include "hardware/driver/driver_shared.h"
#include "hardware/driver/xdrv_03_serial_uart.h"

/*---- TYPES ----*/

/** State of one logical port. */
typedef struct {
    SemaphoreHandle_t      lock;            /* Serialises open / close / write / read / callback of this port */
    bool                   is_open;         /* Set only after the UART started, so a failed open leaves it false */
    int8_t                 tx_pin;          /* From SIG_INIT; GPIO_INVALID when the port is unusable */
    int8_t                 rx_pin;
    int8_t                 de_pin;          /* GPIO_INVALID when the port has no RS485 transceiver */
    HardwareSerial        *serial;          /* Hardware UART taken at open, NULL while closed */
    SerialUartRxCallback_t rx_callback;     /* Called with the received bytes, NULL = nobody listening */
    void                  *rx_arg;          /* Given back to rx_callback on every call */
    uint8_t                rx_buf[SERIAL_UART_RX_CHUNK];    /* Bytes handed to rx_callback */
} SerialUartPort_t;

/*---- STATIC VARIABLES ----*/

static SerialUartPort_t s_ports[SERIAL_UART_PORT_MAX];     /* Port n is s_ports[n - 1] */

static bool s_initialized = false;     /* Set at the end of a successful SIG_INIT */

/*---- STATIC FUNCTIONS ----*/

/**
 * @brief GPIO of the pin option SERIAL_UART<port>_<signal>.
 * @param port   Port number.
 * @param signal "TX", "RX" or "DE".
 * @return GPIO number, or GPIO_INVALID when the option is not set.
 */
static int8_t serial_uart_pin(uint8_t port, const char *signal) {
    String pin_name = "SERIAL_UART" + String(port) + "_" + signal;
    return is_use_name(pin_name.c_str()) ? get_gpio_by_name(pin_name.c_str()) : GPIO_INVALID;
}

/**
 * @brief Stop the UART of a port and give it back (another driver may take it after this).
 * @param serial UART to stop.
 */
static void serial_uart_stop_uart(HardwareSerial *serial) {
    DriverShared_UartLock();
    serial->end();
    DriverShared_UartUnlock();
}

/**
 * @brief UART receive event of a port: hand every waiting byte to the port's callback.
 * @details Runs in the Arduino UART event task, once the line was idle. Without a callback the bytes stay in the
 *          RX buffer for SerialUart_Read().
 * @param port Port that received data.
 */
static void serial_uart_on_receive(uint8_t port) {
    SerialUartPort_t *p = &s_ports[port - 1];

    // Loop: a burst longer than the chunk buffer is handed over in several calls
    for (;;) {
        // Under the lock for the whole chunk, callback included: SerialUart_Close() waits for a running
        // callback, so no callback runs once Close() has returned
        xSemaphoreTake(p->lock, portMAX_DELAY);

        // Step 1: take the next chunk
        if (!p->is_open || p->rx_callback == NULL) {
            xSemaphoreGive(p->lock);
            return;
        }
        size_t available = (size_t)p->serial->available();
        if (available == 0) {
            xSemaphoreGive(p->lock);
            return;
        }
        size_t len = (available > SERIAL_UART_RX_CHUNK) ? SERIAL_UART_RX_CHUNK : available;
        len = p->serial->read(p->rx_buf, len);

        // Step 2: deliver
        p->rx_callback(port, p->rx_buf, len, p->rx_arg);
        xSemaphoreGive(p->lock);
    }
}

/*---- PUBLIC FUNCTIONS ----*/

SerialUartStatus_t SerialUart_Open(uint8_t port, const SerialUartConfig_t *cfg) {
    // Step 1: driver ready, valid arguments, the port has pins (fixed after SIG_INIT)
    if (!s_initialized) return SERIAL_UART_ERR_NOT_INIT;
    if (port < 1 || port > SERIAL_UART_PORT_MAX) return SERIAL_UART_ERR_OUT_OF_RANGE;
    if (cfg == NULL || cfg->baud == 0) return SERIAL_UART_ERR_INVALID_ARG;
    SerialUartPort_t *p = &s_ports[port - 1];
    if (p->tx_pin == GPIO_INVALID) return SERIAL_UART_ERR_NO_PIN;

    xSemaphoreTake(p->lock, portMAX_DELAY);
    if (p->is_open) {
        xSemaphoreGive(p->lock);
        return SERIAL_UART_ERR_ALREADY_OPEN;
    }

    // Step 2: take a free UART and start it, both under the UART lock (Arduino takes RX before TX)
    DriverShared_UartLock();
    HardwareSerial *serial = DriverShared_FindFreeUart();
    if (serial != NULL) {
        serial->begin(cfg->baud, cfg->frame, p->rx_pin, p->tx_pin);
    }
    DriverShared_UartUnlock();
    if (serial == NULL) {
        xSemaphoreGive(p->lock);
        return SERIAL_UART_ERR_NO_FREE_UART;
    }
    if (!(*serial)) {               // operator bool: false when begin() did not install the IDF driver (UART still free)
        xSemaphoreGive(p->lock);
        return SERIAL_UART_ERR_UART;
    }

    // Step 3: received bytes arrive as whole bursts: the UART moves them to the RX buffer only after
    // SERIAL_UART_RX_GAP_CHARS characters of silence (or SERIAL_UART_RX_FIFO_FULL bytes). Must come after begin()
    if (!serial->setRxTimeout(SERIAL_UART_RX_GAP_CHARS) || !serial->setRxFIFOFull(SERIAL_UART_RX_FIFO_FULL)) {
        serial_uart_stop_uart(serial);
        xSemaphoreGive(p->lock);
        return SERIAL_UART_ERR_UART;
    }

    // Step 4: call serial_uart_on_receive() once per finished burst (timeout only)
    serial->onReceive([port]() { serial_uart_on_receive(port); }, true);

    // Step 5: mark the port open LAST, so a reader never sees a half set-up port
    p->serial      = serial;
    p->rx_callback = NULL;
    p->rx_arg      = NULL;
    p->is_open     = true;
    xSemaphoreGive(p->lock);
    return SERIAL_UART_OK;
}

SerialUartStatus_t SerialUart_Close(uint8_t port) {
    // Step 1: driver ready, valid port number
    if (!s_initialized) return SERIAL_UART_ERR_NOT_INIT;
    if (port < 1 || port > SERIAL_UART_PORT_MAX) return SERIAL_UART_ERR_OUT_OF_RANGE;
    SerialUartPort_t *p = &s_ports[port - 1];

    xSemaphoreTake(p->lock, portMAX_DELAY);
    if (!p->is_open) {
        xSemaphoreGive(p->lock);
        return SERIAL_UART_ERR_NOT_OPEN;
    }

    // Step 2: mark the port closed FIRST, so no reader uses a UART that is being shut down
    HardwareSerial *serial = p->serial;
    p->is_open     = false;
    p->serial      = NULL;
    p->rx_callback = NULL;
    p->rx_arg      = NULL;

    // Step 3: stop the receive event, keep the transceiver in receive mode, stop the UART (free again after this)
    serial->onReceive(nullptr);
    if (p->de_pin != GPIO_INVALID) digitalWrite(p->de_pin, LOW);
    serial_uart_stop_uart(serial);

    xSemaphoreGive(p->lock);
    return SERIAL_UART_OK;
}

SerialUartStatus_t SerialUart_Write(uint8_t port, const uint8_t *data, size_t len) {
    // Step 1: driver ready, valid arguments
    if (!s_initialized) return SERIAL_UART_ERR_NOT_INIT;
    if (port < 1 || port > SERIAL_UART_PORT_MAX) return SERIAL_UART_ERR_OUT_OF_RANGE;
    if (data == NULL || len == 0) return SERIAL_UART_ERR_INVALID_ARG;
    SerialUartPort_t *p = &s_ports[port - 1];

    xSemaphoreTake(p->lock, portMAX_DELAY);
    if (!p->is_open) {
        xSemaphoreGive(p->lock);
        return SERIAL_UART_ERR_NOT_OPEN;
    }

    // Step 2: DE high, send, wait until the last bit has left the UART, DE low
    if (p->de_pin != GPIO_INVALID) digitalWrite(p->de_pin, HIGH);
    size_t written = p->serial->write(data, len);
    p->serial->flush();
    if (p->de_pin != GPIO_INVALID) digitalWrite(p->de_pin, LOW);
    xSemaphoreGive(p->lock);

    // Step 3: write() stores less than asked when the TX buffer stays full
    return (written == len) ? SERIAL_UART_OK : SERIAL_UART_ERR_UART;
}

size_t SerialUart_Available(uint8_t port) {
    // Step 1: a port that is not ready or not valid has nothing to read
    if (!s_initialized) return 0;
    if (port < 1 || port > SERIAL_UART_PORT_MAX) return 0;
    SerialUartPort_t *p = &s_ports[port - 1];

    // Step 2: bytes already moved to the RX buffer
    xSemaphoreTake(p->lock, portMAX_DELAY);
    int count = p->is_open ? p->serial->available() : 0;
    xSemaphoreGive(p->lock);
    return (count > 0) ? (size_t)count : 0;
}

SerialUartStatus_t SerialUart_Read(uint8_t port, uint8_t *buf, size_t max_len, size_t *out_len) {
    // Step 1: driver ready, valid arguments; out_len is written on every path
    if (out_len == NULL) return SERIAL_UART_ERR_INVALID_ARG;
    *out_len = 0;
    if (!s_initialized) return SERIAL_UART_ERR_NOT_INIT;
    if (port < 1 || port > SERIAL_UART_PORT_MAX) return SERIAL_UART_ERR_OUT_OF_RANGE;
    if (buf == NULL && max_len > 0) return SERIAL_UART_ERR_INVALID_ARG;
    SerialUartPort_t *p = &s_ports[port - 1];

    xSemaphoreTake(p->lock, portMAX_DELAY);
    if (!p->is_open) {
        xSemaphoreGive(p->lock);
        return SERIAL_UART_ERR_NOT_OPEN;
    }

    // Step 2: take only what is already there; readBytes() would wait for the missing bytes
    size_t count = (size_t)p->serial->available();
    if (count > max_len) count = max_len;
    if (count > 0) *out_len = p->serial->read(buf, count);
    xSemaphoreGive(p->lock);
    return SERIAL_UART_OK;
}

SerialUartStatus_t SerialUart_SetRxCallback(uint8_t port, SerialUartRxCallback_t callback, void *arg) {
    // Step 1: driver ready, valid port number
    if (!s_initialized) return SERIAL_UART_ERR_NOT_INIT;
    if (port < 1 || port > SERIAL_UART_PORT_MAX) return SERIAL_UART_ERR_OUT_OF_RANGE;
    SerialUartPort_t *p = &s_ports[port - 1];

    // Step 2: set or remove it with its arg, under the lock: a running callback never sees one without the other
    xSemaphoreTake(p->lock, portMAX_DELAY);
    if (!p->is_open) {
        xSemaphoreGive(p->lock);
        return SERIAL_UART_ERR_NOT_OPEN;
    }
    p->rx_callback = callback;
    p->rx_arg      = arg;
    xSemaphoreGive(p->lock);
    return SERIAL_UART_OK;
}

const char *SerialUart_StatusToStr(SerialUartStatus_t status) {
    switch (status) {
        case SERIAL_UART_OK:               return "OK";
        case SERIAL_UART_ERR_NOT_INIT:     return "Serial UART not initialized";
        case SERIAL_UART_ERR_OUT_OF_RANGE: return "Serial UART port out of range";
        case SERIAL_UART_ERR_INVALID_ARG:  return "Invalid argument";
        case SERIAL_UART_ERR_NO_PIN:       return "SERIAL_UART<n>_TX / _RX pins not configured";
        case SERIAL_UART_ERR_ALREADY_OPEN: return "Port already open";
        case SERIAL_UART_ERR_NOT_OPEN:     return "Port not open";
        case SERIAL_UART_ERR_NO_FREE_UART: return "No free UART";
        case SERIAL_UART_ERR_UART:         return "UART failed";
        default:                           return "Unknown error";
    }
}

/**
 * @brief Dispatcher entry of the serial UART driver.
 * @details SIG_INIT: create the lock of every port, resolve its pins and park its DE pin in receive mode.
 * @param[in] signal Signal from the dispatcher.
 * @return true if the signal was handled, false otherwise (false at SIG_INIT removes the driver from the dispatcher).
 */
bool Xdrv3(Signal_t signal) {
    switch (signal) {
        case SIG_INIT: {
            for (uint8_t i = 0; i < SERIAL_UART_PORT_MAX; i++) {
                uint8_t           port = i + 1;
                SerialUartPort_t *p    = &s_ports[i];

                // Step 1: the lock that open / close / write / read and the callback of this port share
                p->lock = xSemaphoreCreateMutex();
                if (p->lock == NULL) {
                    LOG_ERROR("Serial UART: cannot create the lock");
                    return false;
                }

                // Step 2: resolve the pins; TX and RX are required (Open only checks tx_pin), DE is optional
                int8_t tx_pin = serial_uart_pin(port, "TX");
                int8_t rx_pin = serial_uart_pin(port, "RX");
                if (rx_pin == GPIO_INVALID) {
                    tx_pin = GPIO_INVALID;
                }
                if (tx_pin != GPIO_INVALID && tx_pin == rx_pin) {
                    LOG_ERROR("SERIAL_UART" + String(port) + ": TX and RX on the same GPIO " + String(tx_pin));
                    tx_pin = GPIO_INVALID;
                }

                // Step 3: the port starts closed
                p->is_open     = false;
                p->tx_pin      = tx_pin;
                p->rx_pin      = rx_pin;
                p->de_pin      = serial_uart_pin(port, "DE");
                p->serial      = NULL;
                p->rx_callback = NULL;
                p->rx_arg      = NULL;

                // Step 4: keep an RS485 transceiver in receive mode until the port sends
                if (p->de_pin != GPIO_INVALID) {
                    pinMode(p->de_pin, OUTPUT);
                    digitalWrite(p->de_pin, LOW);
                }
                if (tx_pin != GPIO_INVALID) {
                    LOG_INFO("SERIAL_UART" + String(port) + " TX GPIO " + String(tx_pin) + " | RX GPIO " +
                             String(rx_pin) + " | DE GPIO " + String(p->de_pin));
                }
            }

            // Step 5: last, so the locks and pins above are ready before anyone may use them
            s_initialized = true;
            return true;
        }
        default:
            return false;
    }
}
