/**
 * @file xdrv_04_serial_modbus.cpp
 * @brief Serial Modbus driver: pins and locks prepared at SIG_INIT, a free hardware UART taken at open and handed
 *        to the Modbus library (see xdrv_04_serial_modbus.h). No CLI command: scripts use it through the modbus
 *        handle.
 */

/*---- INCLUDES ----*/
#include "config.h"
#ifdef USE_MODBUS_MASTER
#include <Arduino.h>
#include <HardwareSerial.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "modbus.h"
#include "core/core_engine.h"
#include "hardware/driver/driver_shared.h"
#include "hardware/driver/xdrv_04_serial_modbus.h"

/*---- TYPES ----*/

/** State of one logical port. */
typedef struct {
    SemaphoreHandle_t lock;         /* Serialises open / close / send of this port */
    bool              is_open;      /* Set only after the stack started, so a failed open leaves it false */
    int8_t            tx_pin;       /* From SIG_INIT; GPIO_INVALID when the port is unusable */
    int8_t            rx_pin;
    int8_t            de_pin;       /* GPIO_INVALID when the port has no RS485 transceiver */
    HardwareSerial   *serial;       /* Hardware UART taken at open, NULL while closed */
    ModbusRS485       mb;           /* Library instance, must live as long as the stack runs */
} SerialModbusPort_t;

/*---- STATIC VARIABLES ----*/

static SerialModbusPort_t s_ports[SERIAL_MODBUS_PORT_MAX];     /* Port n is s_ports[n - 1] */

static bool s_initialized = false;     /* Set at the end of a successful SIG_INIT */

/*---- STATIC FUNCTIONS ----*/

/**
 * @brief GPIO of the pin option SERIAL_MODBUS<port>_<signal>.
 * @param port   Port number.
 * @param signal "TX", "RX" or "DE".
 * @return GPIO number, or GPIO_INVALID when the option is not set.
 */
static int8_t serial_modbus_pin(uint8_t port, const char *signal) {
    String pin_name = "SERIAL_MODBUS" + String(port) + "_" + signal;
    return is_use_name(pin_name.c_str()) ? get_gpio_by_name(pin_name.c_str()) : GPIO_INVALID;
}

/**
 * @brief Translate the library's ModbusSendReceive() result into a serial Modbus status.
 * @param ret Result returned by the library.
 * @return Matching status.
 */
static SerialModbusStatus_t serial_modbus_from_send_result(SendRetType ret) {
    switch (ret) {
        case ESEND_NOERR:     return SERIAL_MODBUS_OK;
        case ESEND_RQINVAL:   return SERIAL_MODBUS_ERR_REQUEST;
        case ESEND_TIMEOUT:   return SERIAL_MODBUS_ERR_TIMEOUT;
        case ESEND_BUFSMALL:  return SERIAL_MODBUS_ERR_BUFFER_TOO_SMALL;
        case ESEND_EXCEPTION: return SERIAL_MODBUS_ERR_EXCEP_UNKNOWN;
        default:              return SERIAL_MODBUS_ERR_RESPONSE;
    }
}

/**
 * @brief Translate a 1-byte Modbus slave exception code into a serial Modbus status.
 * @param code Exception code from the slave (0x01..0x0B).
 * @return Matching exception status.
 */
static SerialModbusStatus_t serial_modbus_from_exception_code(uint8_t code) {
    switch (code) {
        case 1:  return SERIAL_MODBUS_ERR_EXCEP_ILLEGAL_FUNCTION;
        case 2:  return SERIAL_MODBUS_ERR_EXCEP_ILLEGAL_DATA_ADDR;
        case 3:  return SERIAL_MODBUS_ERR_EXCEP_ILLEGAL_DATA_VAL;
        case 4:  return SERIAL_MODBUS_ERR_EXCEP_SLAVE_DEVICE_FAIL;
        case 5:  return SERIAL_MODBUS_ERR_EXCEP_ACKNOWLEDGE;
        case 6:  return SERIAL_MODBUS_ERR_EXCEP_SLAVE_DEVICE_BUSY;
        case 8:  return SERIAL_MODBUS_ERR_EXCEP_MEMORY_PARITY_ERROR;
        case 10: return SERIAL_MODBUS_ERR_EXCEP_GATEWAY_PATH_UNAVAIL;
        case 11: return SERIAL_MODBUS_ERR_EXCEP_GATEWAY_TARGET_FAIL;
        default: return SERIAL_MODBUS_ERR_EXCEP_UNKNOWN;
    }
}

/*---- PUBLIC FUNCTIONS ----*/

SerialModbusStatus_t SerialModbus_Open(uint8_t port, const SerialModbusConfig_t *cfg) {
    // Step 1: driver ready, valid arguments, the port has pins (fixed after SIG_INIT)
    if (!s_initialized) return SERIAL_MODBUS_ERR_NOT_INIT;
    if (port < 1 || port > SERIAL_MODBUS_PORT_MAX) return SERIAL_MODBUS_ERR_OUT_OF_RANGE;
    if (cfg == NULL || cfg->baud == 0) return SERIAL_MODBUS_ERR_INVALID_ARG;
    SerialModbusPort_t *p = &s_ports[port - 1];
    if (p->tx_pin == GPIO_INVALID) return SERIAL_MODBUS_ERR_NO_PIN;

    xSemaphoreTake(p->lock, portMAX_DELAY);
    if (p->is_open) {
        xSemaphoreGive(p->lock);
        return SERIAL_MODBUS_ERR_ALREADY_OPEN;
    }

    // Step 2: take a free UART and start the stack on it, both under the UART lock: ModbusStart() calls begin()
    DriverShared_UartLock();
    HardwareSerial *serial = DriverShared_FindFreeUart();
    bool started = false;
    if (serial != NULL) {
        started = ModbusInit(p->mb, *serial, cfg->baud, cfg->frame, p->tx_pin, p->rx_pin, p->de_pin) &&
                  ModbusStart(p->mb);
        if (!started && (*serial)) {
            serial->end();          // begin() worked but the stack did not start: give the UART back
        }
    }
    DriverShared_UartUnlock();
    if (serial == NULL) {
        xSemaphoreGive(p->lock);
        return SERIAL_MODBUS_ERR_NO_FREE_UART;
    }
    if (!started) {
        xSemaphoreGive(p->lock);
        return SERIAL_MODBUS_ERR_UART;
    }

    // Step 3: mark the port open LAST, so a sender never sees a half set-up port
    p->serial  = serial;
    p->is_open = true;
    xSemaphoreGive(p->lock);
    return SERIAL_MODBUS_OK;
}

SerialModbusStatus_t SerialModbus_Close(uint8_t port) {
    // Step 1: driver ready, valid port number
    if (!s_initialized) return SERIAL_MODBUS_ERR_NOT_INIT;
    if (port < 1 || port > SERIAL_MODBUS_PORT_MAX) return SERIAL_MODBUS_ERR_OUT_OF_RANGE;
    SerialModbusPort_t *p = &s_ports[port - 1];

    // Waits here for a running SerialModbus_Send(): it holds the port lock for the whole transaction
    xSemaphoreTake(p->lock, portMAX_DELAY);
    if (!p->is_open) {
        xSemaphoreGive(p->lock);
        return SERIAL_MODBUS_ERR_NOT_OPEN;
    }

    // Step 2: mark the port closed FIRST
    HardwareSerial *serial = p->serial;
    p->is_open = false;
    p->serial  = NULL;

    // Step 3: stop the receive event (it points to p->mb), then the stack; ModbusDestroy() calls end(), so the
    // UART is free again after this
    serial->onReceive(nullptr);
    DriverShared_UartLock();
    ModbusDestroy(p->mb);
    DriverShared_UartUnlock();

    // Step 4: keep the transceiver in receive mode
    if (p->de_pin != GPIO_INVALID) digitalWrite(p->de_pin, LOW);

    xSemaphoreGive(p->lock);
    return SERIAL_MODBUS_OK;
}

SerialModbusStatus_t SerialModbus_Send(uint8_t port, uint8_t slave, uint8_t function,
                                       uint16_t address, uint16_t quantity, uint32_t timeout_ms,
                                       uint8_t *buf, size_t *len) {
    // Step 1: driver ready, valid arguments
    if (len == NULL) return SERIAL_MODBUS_ERR_INVALID_ARG;
    size_t buf_size = *len;
    *len = 0;
    if (!s_initialized) return SERIAL_MODBUS_ERR_NOT_INIT;
    if (port < 1 || port > SERIAL_MODBUS_PORT_MAX) return SERIAL_MODBUS_ERR_OUT_OF_RANGE;
    if (buf == NULL) return SERIAL_MODBUS_ERR_INVALID_ARG;
    SerialModbusPort_t *p = &s_ports[port - 1];

    // Under the port lock for the whole transaction: Close() cannot destroy the stack while it is in use
    xSemaphoreTake(p->lock, portMAX_DELAY);
    if (!p->is_open) {
        xSemaphoreGive(p->lock);
        return SERIAL_MODBUS_ERR_NOT_OPEN;
    }

    // Step 2: send, wait and copy the answer out in one library call
    USHORT answer_len = (USHORT)buf_size;
    SendRetType ret = ModbusSendReceive(p->mb, slave, function, address, quantity, (INT)timeout_ms, buf, &answer_len);
    xSemaphoreGive(p->lock);

    // Step 3: the slave answered with an exception: translate its 1-byte code, no data
    if (ret == ESEND_EXCEPTION) {
        uint8_t code = (answer_len >= 1) ? buf[0] : 0;
        return serial_modbus_from_exception_code(code);
    }

    // Step 4: report the length the library stored (only on success)
    if (ret == ESEND_NOERR) *len = answer_len;
    return serial_modbus_from_send_result(ret);
}

const char *SerialModbus_StatusToStr(SerialModbusStatus_t status) {
    switch (status) {
        case SERIAL_MODBUS_OK:                           return "OK";
        case SERIAL_MODBUS_ERR_NOT_INIT:                 return "Serial Modbus not initialized";
        case SERIAL_MODBUS_ERR_OUT_OF_RANGE:             return "Serial Modbus port out of range";
        case SERIAL_MODBUS_ERR_INVALID_ARG:              return "Invalid argument";
        case SERIAL_MODBUS_ERR_NO_PIN:                   return "SERIAL_MODBUS<n>_TX / _RX pins not configured";
        case SERIAL_MODBUS_ERR_ALREADY_OPEN:             return "Port already open";
        case SERIAL_MODBUS_ERR_NOT_OPEN:                 return "Port not open";
        case SERIAL_MODBUS_ERR_NO_FREE_UART:             return "No free UART";
        case SERIAL_MODBUS_ERR_UART:                     return "UART or Modbus stack failed to start";
        case SERIAL_MODBUS_ERR_REQUEST:                  return "Invalid request";
        case SERIAL_MODBUS_ERR_TIMEOUT:                  return "Timeout";
        case SERIAL_MODBUS_ERR_RESPONSE:                 return "Invalid response";
        case SERIAL_MODBUS_ERR_BUFFER_TOO_SMALL:         return "Response buffer too small";
        case SERIAL_MODBUS_ERR_EXCEP_ILLEGAL_FUNCTION:     return "Slave exception: Illegal function (0x01)";
        case SERIAL_MODBUS_ERR_EXCEP_ILLEGAL_DATA_ADDR:    return "Slave exception: Illegal data address (0x02)";
        case SERIAL_MODBUS_ERR_EXCEP_ILLEGAL_DATA_VAL:     return "Slave exception: Illegal data value (0x03)";
        case SERIAL_MODBUS_ERR_EXCEP_SLAVE_DEVICE_FAIL:    return "Slave exception: Slave device failure (0x04)";
        case SERIAL_MODBUS_ERR_EXCEP_ACKNOWLEDGE:          return "Slave exception: Acknowledge (0x05)";
        case SERIAL_MODBUS_ERR_EXCEP_SLAVE_DEVICE_BUSY:    return "Slave exception: Slave device busy (0x06)";
        case SERIAL_MODBUS_ERR_EXCEP_MEMORY_PARITY_ERROR:  return "Slave exception: Memory parity error (0x08)";
        case SERIAL_MODBUS_ERR_EXCEP_GATEWAY_PATH_UNAVAIL: return "Slave exception: Gateway path unavailable (0x0A)";
        case SERIAL_MODBUS_ERR_EXCEP_GATEWAY_TARGET_FAIL:  return "Slave exception: Gateway target device failed to respond (0x0B)";
        case SERIAL_MODBUS_ERR_EXCEP_UNKNOWN:              return "Slave exception: Unknown";
        default:                                         return "Unknown error";
    }
}

/**
 * @brief Dispatcher entry of the serial Modbus driver.
 * @details SIG_INIT: create the lock of every port, resolve its pins and park its DE pin in receive mode.
 * @param[in] signal Signal from the dispatcher.
 * @return true if the signal was handled, false otherwise (false at SIG_INIT removes the driver from the dispatcher).
 */
bool Xdrv4(Signal_t signal) {
    switch (signal) {
        case SIG_INIT: {
            for (uint8_t i = 0; i < SERIAL_MODBUS_PORT_MAX; i++) {
                uint8_t             port = i + 1;
                SerialModbusPort_t *p    = &s_ports[i];

                // Step 1: the lock that open / close / send of this port share
                p->lock = xSemaphoreCreateMutex();
                if (p->lock == NULL) {
                    LOG_ERROR("Serial Modbus: cannot create the lock");
                    return false;
                }

                // Step 2: resolve the pins; TX and RX are required (Open only checks tx_pin), DE is optional
                int8_t tx_pin = serial_modbus_pin(port, "TX");
                int8_t rx_pin = serial_modbus_pin(port, "RX");
                if (rx_pin == GPIO_INVALID) {
                    tx_pin = GPIO_INVALID;
                }
                if (tx_pin != GPIO_INVALID && tx_pin == rx_pin) {
                    LOG_ERROR("SERIAL_MODBUS" + String(port) + ": TX and RX on the same GPIO " + String(tx_pin));
                    tx_pin = GPIO_INVALID;
                }

                // Step 3: the port starts closed
                p->is_open = false;
                p->tx_pin  = tx_pin;
                p->rx_pin  = rx_pin;
                p->de_pin  = serial_modbus_pin(port, "DE");
                p->serial  = NULL;

                // Step 4: keep an RS485 transceiver in receive mode until the port sends
                if (p->de_pin != GPIO_INVALID) {
                    pinMode(p->de_pin, OUTPUT);
                    digitalWrite(p->de_pin, LOW);
                }
                if (tx_pin != GPIO_INVALID) {
                    LOG_INFO("SERIAL_MODBUS" + String(port) + " TX GPIO " + String(tx_pin) + " | RX GPIO " +
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

#endif // USE_MODBUS_MASTER
