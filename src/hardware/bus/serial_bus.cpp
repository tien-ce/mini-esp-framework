/**
 * @file serial_bus.cpp
 * @brief Hardware arbiter of the board's serial ports (see serial_bus.h).
 *
 * The bus knows nothing about protocols. It resolves the GPIOs and the UART of a port and keeps
 * track of who owns it. Acquire / Release / queries are added problem by problem.
 */

#include "hardware/bus/serial_bus.h"
#include "core/pin_config.h"
#include "core/core_log.h"
#include <string.h>
#include <stdio.h>
#include <mutex>

/* ---------------------------------------------------------------------------
 * Constants
 * ------------------------------------------------------------------------- */

/* Room for "SERIAL<n>_TX" with a 3 digit port number and the terminator. */
#define PIN_NAME_LEN    16

/* ---------------------------------------------------------------------------
 * Types
 * ------------------------------------------------------------------------- */

/* Everything the bus knows about one port. A port is free when ref_count is 0. */
typedef struct {
    char             owner[SERIAL_BUS_OWNER_NAME_MAX + 1];  /* COPY of the owner name, empty when free */
    uint8_t          ref_count;                             /* Number of acquires currently holding the port */
    SerialResources_t hw;                                   /* Resolved at the first acquire, kept until the last release */
} Port_t;

/* ---------------------------------------------------------------------------
 * State
 * ------------------------------------------------------------------------- */

/* One entry per port. Index = port number - 1 (ports start at 1). */
static Port_t s_ports[SERIAL_BUS_PORT_COUNT];

/* Port n is UART n. UART0 is the console, so the table starts at Serial1.
 * Only the first SERIAL_BUS_PORT_COUNT entries are ever used. */
static HardwareSerial *const s_uarts[] = { &Serial1, &Serial2 };

/* Protects s_ports. One lock shared by all ports: acquire/release are rare and short. */
static std::mutex s_lock;


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
 * @brief Look up the GPIO assigned to a pin configuration name SERIAL<port>_<signal>.
 * @param port Port number.
 * @param signal Signal suffix: "TX", "RX" or "DE".
 * @return GPIO number, or GPIO_INVALID when the name is not assigned or did not resolve.
 */
static int8_t PinOf(uint8_t port, const char *signal) {
    // Step 1: build the name without touching the heap
    char name[PIN_NAME_LEN];
    snprintf(name, sizeof(name), "SERIAL%u_%s", (unsigned)port, signal);

    // Step 2: ask the pin configuration
    String key(name);
    if (!is_use_name(key)) return GPIO_INVALID;
    return get_gpio_by_name(key);
}

/**
 * @brief Resolve the UART and the GPIOs of a port from the pin configuration.
 * @details TX and RX are required. DE is optional: a plain TTL / RS232 port has no transceiver.
 *          *hw is written only on success.
 * @param port Port number, 1..SERIAL_BUS_PORT_COUNT.
 * @param hw Receives the resolved resources.
 * @return SERIAL_BUS_OK, SERIAL_BUS_ERR_INVALID_PORT or SERIAL_BUS_ERR_NO_PINS.
 */
static SerialBusStatus_t ResolveResources(uint8_t port, SerialResources_t *hw) {
    // Step 1: the port number must exist on this chip
    if (!ValidPort(port)) return SERIAL_BUS_ERR_INVALID_PORT;

    // Step 2: TX and RX are required
    int8_t tx = PinOf(port, "TX");
    int8_t rx = PinOf(port, "RX");
    if (tx == GPIO_INVALID || rx == GPIO_INVALID) {
        LOG_ERROR(String("[SerialBus] Port ") + String(port) + ": SERIAL" + String(port) + "_TX / _RX not configured");
        return SERIAL_BUS_ERR_NO_PINS;
    }

    // Step 3: the same GPIO cannot be both lines
    if (tx == rx) {
        LOG_ERROR(String("[SerialBus] Port ") + String(port) + ": TX and RX use the same GPIO " + String(tx));
        return SERIAL_BUS_ERR_NO_PINS;
    }

    // Step 4: DE is optional, GPIO_INVALID when missing
    hw->serial = s_uarts[port - 1];
    hw->tx_pin = tx;
    hw->rx_pin = rx;
    hw->de_pin = PinOf(port, "DE");
    return SERIAL_BUS_OK;
}

/* ---------------------------------------------------------------------------
 * Public: lifecycle
 * ------------------------------------------------------------------------- */

SerialBusStatus_t SerialBus_Acquire(uint8_t port, const char *owner_name, const SerialResources_t **out_res) {
    // Step 1: validate the arguments before touching any state
    if (!ValidPort(port)) return SERIAL_BUS_ERR_INVALID_PORT;
    if (owner_name == NULL) return SERIAL_BUS_ERR_INVALID_ARG;
    size_t nameLen = strlen(owner_name);
    if (nameLen == 0 || nameLen > SERIAL_BUS_OWNER_NAME_MAX) return SERIAL_BUS_ERR_INVALID_ARG;

    std::lock_guard<std::mutex> guard(s_lock);
    Port_t *p = &s_ports[port - 1];

    // Step 2: the port is held already: only the same owner may share it
    if (p->ref_count > 0) {
        if (strcmp(p->owner, owner_name) != 0) {
            LOG_WARNING(String("[SerialBus] Port ") + String(port) + " is held by '" + p->owner +
                        "', refused '" + owner_name + "'");
            return SERIAL_BUS_ERR_CONFLICT;
        }
        p->ref_count++;
        *out_res = &p->hw;
        return SERIAL_BUS_ALREADY_ACQUIRED;
    }

    // Step 3: free port: resolve the resources first, the state is written only after it succeeded
    SerialResources_t res;
    SerialBusStatus_t status = ResolveResources(port, &res);
    if (status != SERIAL_BUS_OK) return status;

    // Step 4: record the claim (the name is copied, the caller's buffer may disappear)
    memcpy(p->owner, owner_name, nameLen + 1);
    p->hw = res;
    p->ref_count = 1;
    *out_res = &p->hw;

    LOG_INFO(String("[SerialBus] Port ") + String(port) + " acquired by '" + p->owner + "' | TX: " +
             String(res.tx_pin) + " | RX: " + String(res.rx_pin) + " | DE: " + String(res.de_pin));
    return SERIAL_BUS_OK;
}

SerialBusStatus_t SerialBus_Release(uint8_t port, const char *owner_name) {
    // Step 1: validate the arguments before touching any state
    if (!ValidPort(port)) return SERIAL_BUS_ERR_INVALID_PORT;
    if (owner_name == NULL) return SERIAL_BUS_ERR_INVALID_ARG;
    size_t nameLen = strlen(owner_name);
    if (nameLen == 0 || nameLen > SERIAL_BUS_OWNER_NAME_MAX) return SERIAL_BUS_ERR_INVALID_ARG;

    std::lock_guard<std::mutex> guard(s_lock);
    Port_t *p = &s_ports[port - 1];

    // Step 2: only the owner that holds the port can release it (also stops ref_count from going below 0)
    if (p->ref_count == 0 || strcmp(p->owner, owner_name) != 0) return SERIAL_BUS_ERR_NOT_OWNER;

    // Step 3: other acquires of the same owner are still using the port: keep it
    p->ref_count--;
    if (p->ref_count > 0) return SERIAL_BUS_ALREADY_ACQUIRED;

    // Step 4: last release: the port is free again
    LOG_INFO(String("[SerialBus] Port ") + String(port) + " released by '" + p->owner + "'");
    memset(p, 0, sizeof(*p));
    return SERIAL_BUS_OK;
}

/* ---------------------------------------------------------------------------
 * Public: query
 * ------------------------------------------------------------------------- */

const char *SerialBus_GetOwner(uint8_t port) {
    // Step 1: an invalid port has no owner
    if (!ValidPort(port)) return NULL;

    // Step 2: read under the bus lock so a concurrent acquire/release is not seen half done
    std::lock_guard<std::mutex> guard(s_lock);
    Port_t *p = &s_ports[port - 1];
    return (p->ref_count > 0) ? p->owner : NULL;
}

const char *SerialBus_StatusToStr(SerialBusStatus_t status) {
    switch (status) {
        case SERIAL_BUS_OK:               return "OK";
        case SERIAL_BUS_ALREADY_ACQUIRED: return "Already acquired";
        case SERIAL_BUS_ERR_CONFLICT:     return "Port held by another owner";
        case SERIAL_BUS_ERR_INVALID_PORT: return "Invalid serial port";
        case SERIAL_BUS_ERR_INVALID_ARG:  return "Invalid argument";
        case SERIAL_BUS_ERR_NO_PINS:      return "SERIAL<n>_TX / _RX pins not configured";
        case SERIAL_BUS_ERR_NOT_OWNER:    return "Caller does not hold the port";
        default:                          return "Unknown";
    }
}
