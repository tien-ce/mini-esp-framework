/*
 * @file serial_raw.cpp
 * @brief Raw serial protocol layer (see serial_raw.h).
 *
 * Keeps a small state record per port and talks to the hardware only through serial_bus.
 * Open / Close / Write / Read / Transact are added problem by problem.
 */

#include "hardware/protocol/serial_raw.h"
#include "core/pin_config.h"
#include <string.h>
#include <mutex>

/* ---------------------------------------------------------------------------
 * Types
 * ------------------------------------------------------------------------- */

/* State of one port. */
typedef struct {
    bool                     opened;    /* Set only AFTER begin() succeeded, so a failed open leaves it false */
    const SerialResources_t *res;       /* Resources owned by the bus, valid while opened */
    std::mutex               lock;      /* Serialises open / close / write / read / transact on this port */
    SerialRawRxCb_t          rx_cb;     /* Called with the received bytes, NULL = nobody listening */
    uint8_t                  rx_buf[SERIAL_RAW_RX_CHUNK]; /* Bytes handed to rx_cb */
} RawPort_t;

/* ---------------------------------------------------------------------------
 * State
 * ------------------------------------------------------------------------- */

/* One entry per port. Index = port number - 1 (ports start at 1). */
static RawPort_t s_raw[SERIAL_BUS_PORT_COUNT];

/* ---------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

/**
 * @brief Check that a port number is valid for this chip.
 * @param port Port number to check.
 * @return True when 1 <= port <= SERIAL_BUS_PORT_COUNT.
 */
static bool ValidPort(uint8_t port) {
    return port >= 1 && port <= SERIAL_BUS_PORT_COUNT;
}

/**
 * @brief Translate a serial bus status of an acquire into a serial_raw status.
 * @param status Status returned by SerialBus_Acquire.
 * @return Matching serial_raw status.
 */
static SerialRawStatus_t FromBusStatus(SerialBusStatus_t status) {
    switch (status) {
        case SERIAL_BUS_ERR_CONFLICT: return SERIAL_RAW_ERR_PORT_BUSY;
        case SERIAL_BUS_ERR_NO_PINS:  return SERIAL_RAW_ERR_NO_PINS;
        default:                      return SERIAL_RAW_ERR_INVALID_ARG;
    }
}

/**
 * @brief UART receive event of a port: hand every waiting byte to the port's callback.
 * @details Runs in the Arduino UART event task, once the line was idle. Without a callback the bytes stay
 *          in the RX buffer for SerialRaw_Read().
 * @param port Port that received data.
 */
static void OnRx(uint8_t port) {
    RawPort_t *rp = &s_raw[port - 1];

    for (;;) {
        // Step 1: under the lock, take the next chunk (the callback itself runs without it)
        size_t len;
        SerialRawRxCb_t cb;
        {
            std::lock_guard<std::mutex> guard(rp->lock);
            if (!rp->opened || rp->rx_cb == NULL) return;
            size_t avail = (size_t)rp->res->serial->available();
            if (avail == 0) return;
            len = (avail > SERIAL_RAW_RX_CHUNK) ? SERIAL_RAW_RX_CHUNK : avail;
            len = rp->res->serial->read(rp->rx_buf, len);
            cb = rp->rx_cb;
        }

        // Step 2: deliver
        cb(port, rp->rx_buf, len);
    }
}

/* ---------------------------------------------------------------------------
 * Public
 * ------------------------------------------------------------------------- */

bool SerialRaw_FrameFromStr(const char *name, uint32_t *out) {
    static const struct {
        const char *name;
        uint32_t    frame;
    } table[] = {
        { "8N1", SERIAL_8N1 }, { "8N2", SERIAL_8N2 }, { "8E1", SERIAL_8E1 }, { "8E2", SERIAL_8E2 },
        { "8O1", SERIAL_8O1 }, { "8O2", SERIAL_8O2 }, { "7N1", SERIAL_7N1 }, { "7N2", SERIAL_7N2 },
        { "7E1", SERIAL_7E1 }, { "7E2", SERIAL_7E2 }, { "7O1", SERIAL_7O1 }, { "7O2", SERIAL_7O2 },
    };
    if (name == NULL || out == NULL) return false;
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (strcmp(name, table[i].name) == 0) {
            *out = table[i].frame;
            return true;
        }
    }
    return false;
}

SerialRawStatus_t SerialRaw_Open(uint8_t port, const SerialRawConfig_t *cfg) {
    // Step 1: validate the arguments before touching any state
    if (!ValidPort(port)) return SERIAL_RAW_ERR_INVALID_PORT;
    if (cfg == NULL || cfg->baud == 0) return SERIAL_RAW_ERR_INVALID_ARG;

    RawPort_t *rp = &s_raw[port - 1];
    std::lock_guard<std::mutex> guard(rp->lock);

    // Step 2: only one caller per port: answer here, the bus is not involved
    if (rp->opened) return SERIAL_RAW_ERR_ALREADY_OPEN;

    // Step 3: claim the port on the bus
    const SerialResources_t *res = NULL;
    SerialBusStatus_t busStatus = SerialBus_Acquire(port, SERIAL_RAW_OWNER_NAME, &res);
    if (busStatus != SERIAL_BUS_OK) return FromBusStatus(busStatus);

    // Step 4: open the UART (Arduino takes RX before TX); give the port back if it did not start
    res->serial->begin(cfg->baud, cfg->frame, res->rx_pin, res->tx_pin);
    if (!(*res->serial)) { // Like if (!Serial1) called after Serial.begin (This is wirten by overload operator bool of an object instance.
        SerialBus_Release(port, SERIAL_RAW_OWNER_NAME);
        return SERIAL_RAW_ERR_UART;
    }

    // Step 5: [serial_raw RX config] make received bytes arrive as whole bursts: the UART moves them to the RX
    // buffer only after SERIAL_RAW_RX_GAP_CHARS characters of silence (or SERIAL_RAW_RX_FIFO_FULL bytes), so
    // Available() > 0 means a finished burst. Must come after begin() (the UART driver must exist); the
    // defaults of the core are not enough: at low baud it would move every byte at once.
    if (!res->serial->setRxTimeout(SERIAL_RAW_RX_GAP_CHARS) ||
        !res->serial->setRxFIFOFull(SERIAL_RAW_RX_FIFO_FULL)) {
        res->serial->end();
        SerialBus_Release(port, SERIAL_RAW_OWNER_NAME);
        return SERIAL_RAW_ERR_UART;
    }

    // Step 6: [serial_raw RX callback] call OnRx() once per finished burst (timeout only, after the RX config above)
    res->serial->onReceive([port]() { OnRx(port); }, true);

    // Step 7: park the transceiver in receive mode so the bus is never driven by accident
    if (res->de_pin != GPIO_INVALID) {
        pinMode(res->de_pin, OUTPUT);
        digitalWrite(res->de_pin, LOW);
    }

    // Step 8: mark the port open LAST, so a reader never sees a half set-up port
    rp->res = res;
    rp->rx_cb = NULL;
    rp->opened = true;
    return SERIAL_RAW_OK;
}

SerialRawStatus_t SerialRaw_Close(uint8_t port) {
    // Step 1: validate the port before touching any state
    if (!ValidPort(port)) return SERIAL_RAW_ERR_INVALID_PORT;

    RawPort_t *rp = &s_raw[port - 1];
    std::lock_guard<std::mutex> guard(rp->lock);

    // Step 2: nothing to close if the port was never opened (or begin() failed)
    if (!rp->opened) return SERIAL_RAW_ERR_NOT_OPEN;

    // Step 3: mark the port closed FIRST, so no reader uses a UART that is being shut down
    const SerialResources_t *res = rp->res;
    rp->opened = false;
    rp->res = NULL;

    // Step 4: stop the receive callback, then leave the transceiver in receive mode and stop the UART
    rp->rx_cb = NULL;
    res->serial->onReceive(nullptr);
    if (res->de_pin != GPIO_INVALID) digitalWrite(res->de_pin, LOW);
    res->serial->end();

    // Step 5: give the port back to the bus
    SerialBus_Release(port, SERIAL_RAW_OWNER_NAME);
    return SERIAL_RAW_OK;
}

SerialRawStatus_t SerialRaw_Write(uint8_t port, const uint8_t *data, size_t len) {
    // Step 1: validate the arguments before touching any state
    if (!ValidPort(port)) return SERIAL_RAW_ERR_INVALID_PORT;
    if (data == NULL || len == 0) return SERIAL_RAW_ERR_INVALID_ARG;

    RawPort_t *rp = &s_raw[port - 1];
    std::lock_guard<std::mutex> guard(rp->lock);

    // Step 2: the port must be open
    if (!rp->opened) return SERIAL_RAW_ERR_NOT_OPEN;
    const SerialResources_t *res = rp->res;

    // Step 3: talk: DE high, send, wait until the last bit has left the UART, DE low
    if (res->de_pin != GPIO_INVALID) digitalWrite(res->de_pin, HIGH);
    size_t written = res->serial->write(data, len);
    res->serial->flush();
    if (res->de_pin != GPIO_INVALID) digitalWrite(res->de_pin, LOW);

    // Step 4: write() stores less than asked when the TX buffer stays full
    return (written == len) ? SERIAL_RAW_OK : SERIAL_RAW_ERR_UART;
}

size_t SerialRaw_Available(uint8_t port) {
    // Step 1: an invalid or closed port has nothing to read
    if (!ValidPort(port)) return 0;

    RawPort_t *rp = &s_raw[port - 1];
    std::lock_guard<std::mutex> guard(rp->lock);
    if (!rp->opened) return 0;

    // Step 2: bytes already moved to the RX buffer
    int n = rp->res->serial->available();
    return (n > 0) ? (size_t)n : 0;
}

SerialRawStatus_t SerialRaw_Read(uint8_t port, uint8_t *buf, size_t max_len, size_t *out_len) {
    // Step 1: out_len is written on every path
    *out_len = 0;

    // Step 2: validate the arguments before touching any state
    if (!ValidPort(port)) return SERIAL_RAW_ERR_INVALID_PORT;

    RawPort_t *rp = &s_raw[port - 1];
    std::lock_guard<std::mutex> guard(rp->lock);

    // Step 3: the port must be open
    if (!rp->opened) return SERIAL_RAW_ERR_NOT_OPEN;

    // Step 4: nothing to take when max_len is 0
    if (max_len == 0) return SERIAL_RAW_OK;

    // Step 5: take only what is already there; readBytes() would wait for the missing bytes
    HardwareSerial *serial = rp->res->serial;
    size_t count = (size_t)serial->available();
    if (count > max_len) count = max_len;
    if (count > 0) *out_len = serial->read(buf, count);
    return SERIAL_RAW_OK;
}

SerialRawStatus_t SerialRaw_SetRxCallback(uint8_t port, SerialRawRxCb_t cb) {
    // Step 1: validate the port before touching any state
    if (!ValidPort(port)) return SERIAL_RAW_ERR_INVALID_PORT;

    RawPort_t *rp = &s_raw[port - 1];
    std::lock_guard<std::mutex> guard(rp->lock);

    // Step 2: the port must be open (Close clears the callback)
    if (!rp->opened) return SERIAL_RAW_ERR_NOT_OPEN;

    // Step 3: set or remove it
    rp->rx_cb = cb;
    return SERIAL_RAW_OK;
}

const char *SerialRaw_StatusToStr(SerialRawStatus_t status) {
    switch (status) {
        case SERIAL_RAW_OK:                return "OK";
        case SERIAL_RAW_ERR_INVALID_PORT:  return "Invalid serial port";
        case SERIAL_RAW_ERR_INVALID_ARG:   return "Invalid argument";
        case SERIAL_RAW_ERR_PORT_BUSY:     return "Port used by another owner";
        case SERIAL_RAW_ERR_ALREADY_OPEN:  return "Port already open";
        case SERIAL_RAW_ERR_NO_PINS:       return "SERIAL<n>_TX / _RX pins not configured";
        case SERIAL_RAW_ERR_NOT_OPEN:      return "Port not open";
        case SERIAL_RAW_ERR_UART:          return "UART failed to start";
        default:                           return "Unknown";
    }
}
