/*
 * @file modbus_master.cpp
 * @brief Modbus master protocol layer (see modbus_master.h).
 *
 * Keeps one Modbus library instance per port and talks to the hardware only through serial_bus.
 * Open / Close / IsOpen are written first, the transaction functions are added problem by problem.
 */

#include "config.h"
#ifdef USE_MODBUS_MASTER
#include "hardware/protocol/modbus_master.h"
#include "core/pin_config.h"
#include "modbus.h"
#include <string.h>
#include <mutex>

/* ---------------------------------------------------------------------------
 * Types
 * ------------------------------------------------------------------------- */

/* State of one port. */
typedef struct {
    ModbusRS485 mb;         /* Library instance, must live as long as the stack runs */
    bool        opened;     /* Set only AFTER the stack started, so a failed open leaves it false */
    std::mutex  lock;       /* Serialises open / close on this port */
} ModbusPort_t;

/* ---------------------------------------------------------------------------
 * State
 * ------------------------------------------------------------------------- */

/* One entry per port. Index = port number - 1 (ports start at 1). */
static ModbusPort_t s_mb[SERIAL_BUS_PORT_COUNT];

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
 * @brief Translate a serial bus status of an acquire into a modbus_master status.
 * @param status Status returned by SerialBus_Acquire.
 * @return Matching modbus_master status.
 */
static ModbusMasterStatus_t FromBusStatus(SerialBusStatus_t status) {
    switch (status) {
        case SERIAL_BUS_ERR_CONFLICT: return MBM_ERR_PORT_BUSY;
        case SERIAL_BUS_ERR_NO_PINS:  return MBM_ERR_NO_PINS;
        default:                      return MBM_ERR_INVALID_ARG;
    }
}

/**
 * @brief Translate the library's ModbusSendReceive() result into a modbus_master status.
 * @param ret Result returned by the library.
 * @return Matching modbus_master status.
 */
static ModbusMasterStatus_t FromSendResult(SendRetType ret) {
    switch (ret) {
        case ESEND_NOERR:     return MBM_OK;
        case ESEND_RQINVAL:   return MBM_ERR_REQUEST;
        case ESEND_TIMEOUT:   return MBM_ERR_TIMEOUT;
        case ESEND_BUFSMALL:  return MBM_ERR_BUFFER_TOO_SMALL;
        case ESEND_EXCEPTION: return MBM_ERR_EXCEP_UNKNOWN;
        default:              return MBM_ERR_RESPONSE;
    }
}

/**
 * @brief Translate a 1-byte Modbus slave exception code into a modbus_master status.
 * @param code Exception code from slave (0x01..0x0B).
 * @return Matching modbus_master exception status.
 */
static ModbusMasterStatus_t FromExceptionCode(uint8_t code) {
    switch (code) {
        case 1:  return MBM_ERR_EXCEP_ILLEGAL_FUNCTION;
        case 2:  return MBM_ERR_EXCEP_ILLEGAL_DATA_ADDR;
        case 3:  return MBM_ERR_EXCEP_ILLEGAL_DATA_VAL;
        case 4:  return MBM_ERR_EXCEP_SLAVE_DEVICE_FAIL;
        case 5:  return MBM_ERR_EXCEP_ACKNOWLEDGE;
        case 6:  return MBM_ERR_EXCEP_SLAVE_DEVICE_BUSY;
        case 8:  return MBM_ERR_EXCEP_MEMORY_PARITY_ERROR;
        case 10: return MBM_ERR_EXCEP_GATEWAY_PATH_UNAVAIL;
        case 11: return MBM_ERR_EXCEP_GATEWAY_TARGET_FAIL;
        default: return MBM_ERR_EXCEP_UNKNOWN;
    }
}

/* ---------------------------------------------------------------------------
 * Public
 * ------------------------------------------------------------------------- */

bool ModbusMaster_FrameFromStr(const char *name, ModbusSerialConfig_t *out) {
    static const struct {
        const char          *name;
        ModbusSerialConfig_t frame;
    } table[] = {
        { "8N1", RTU_8N1 }, { "8N2", RTU_8N2 }, { "8E1", RTU_8E1 }, { "8E2", RTU_8E2 },
        { "8O1", RTU_8O1 }, { "8O2", RTU_8O2 }, { "7N1", RTU_7N1 }, { "7N2", RTU_7N2 },
        { "7E1", RTU_7E1 }, { "7E2", RTU_7E2 }, { "7O1", RTU_7O1 }, { "7O2", RTU_7O2 },
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

ModbusMasterStatus_t ModbusMaster_Open(uint8_t port, uint32_t baud, ModbusSerialConfig_t frame) {
    // Step 1: validate the arguments before touching any state
    if (!ValidPort(port)) return MBM_ERR_INVALID_PORT;
    if (baud == 0) return MBM_ERR_INVALID_ARG;

    ModbusPort_t *mp = &s_mb[port - 1];
    std::lock_guard<std::mutex> guard(mp->lock);

    // Step 2: one open per port: answer here, the bus is not involved
    if (mp->opened) return MBM_ERR_ALREADY_OPEN;

    // Step 3: claim the port on the bus
    const SerialResources_t *res = NULL;
    SerialBusStatus_t busStatus = SerialBus_Acquire(port, MODBUS_MASTER_OWNER_NAME, &res);
    if (busStatus != SERIAL_BUS_OK) return FromBusStatus(busStatus);

    // Step 4: hand the pins to the library (it opens the UART and drives DE itself); give the port back if it fails
    if (!ModbusInit(mp->mb, *res->serial, baud, frame, res->tx_pin, res->rx_pin, res->de_pin) ||
        !ModbusStart(mp->mb)) {
        SerialBus_Release(port, MODBUS_MASTER_OWNER_NAME);
        return MBM_ERR_UART;
    }

    // Step 5: mark the port open LAST, so a caller never sees a half set-up port
    mp->opened = true;
    return MBM_OK;
}

ModbusMasterStatus_t ModbusMaster_Close(uint8_t port) {
    // Step 1: validate the port before touching any state
    if (!ValidPort(port)) return MBM_ERR_INVALID_PORT;

    ModbusPort_t *mp = &s_mb[port - 1];
    std::lock_guard<std::mutex> guard(mp->lock);

    // Step 2: nothing to close if the port was never opened
    if (!mp->opened) return MBM_ERR_NOT_OPEN;

    // Step 3: mark the port closed FIRST, so no caller starts a transaction on a stack being shut down
    mp->opened = false;

    // Step 4: Destroy the modbus
    ModbusDestroy(mp->mb);

    // Step 5: give the port back to the bus
    SerialBus_Release(port, MODBUS_MASTER_OWNER_NAME);
    return MBM_OK;
}

bool ModbusMaster_IsOpen(uint8_t port) {
    if (!ValidPort(port)) return false;
    return s_mb[port - 1].opened;
}

ModbusMasterStatus_t ModbusMaster_Send(uint8_t port, uint8_t slave, uint8_t function,
                                       uint16_t address, uint16_t quantity, uint32_t timeout_ms,
                                       uint8_t *buf, size_t *len) {
    // Step 1: the port must be valid and open (the library serialises the transactions, no lock here)
    if (!ValidPort(port)) return MBM_ERR_INVALID_PORT;
    ModbusPort_t *mp = &s_mb[port - 1];
    if (!mp->opened) return MBM_ERR_NOT_OPEN;

    // Step 2: send, wait and copy the answer out in one library call
    USHORT answer_len = (USHORT)*len;
    SendRetType ret = ModbusSendReceive(mp->mb, slave, function, address, quantity, (INT)timeout_ms, buf, &answer_len);

    // Step 3: if slave returned an exception, translate the 1-byte code and clear len
    if (ret == ESEND_EXCEPTION) {
        uint8_t code = (answer_len >= 1 && buf != NULL) ? buf[0] : 0;
        *len = 0;
        return FromExceptionCode(code);
    }

    // Step 4: report the length the library stored and our own status
    *len = answer_len;
    return FromSendResult(ret);
}

const char *ModbusMaster_StatusToStr(ModbusMasterStatus_t status) {
    switch (status) {
        case MBM_OK:               return "OK";
        case MBM_ERR_INVALID_PORT: return "Invalid serial port";
        case MBM_ERR_INVALID_ARG:  return "Invalid argument";
        case MBM_ERR_PORT_BUSY:    return "Port used by another owner";
        case MBM_ERR_ALREADY_OPEN: return "Port already open";
        case MBM_ERR_NO_PINS:      return "SERIAL<n>_TX / _RX pins not configured";
        case MBM_ERR_NOT_OPEN:     return "Port not open";
        case MBM_ERR_UART:         return "UART or Modbus stack failed to start";
        case MBM_ERR_REQUEST:      return "Invalid request";
        case MBM_ERR_TIMEOUT:      return "Timeout";
        case MBM_ERR_RESPONSE:     return "Invalid response";
        case MBM_ERR_BUFFER_TOO_SMALL: return "Response buffer too small";
        case MBM_ERR_EXCEP_ILLEGAL_FUNCTION:     return "Slave exception: Illegal function (0x01)";
        case MBM_ERR_EXCEP_ILLEGAL_DATA_ADDR:    return "Slave exception: Illegal data address (0x02)";
        case MBM_ERR_EXCEP_ILLEGAL_DATA_VAL:     return "Slave exception: Illegal data value (0x03)";
        case MBM_ERR_EXCEP_SLAVE_DEVICE_FAIL:    return "Slave exception: Slave device failure (0x04)";
        case MBM_ERR_EXCEP_ACKNOWLEDGE:          return "Slave exception: Acknowledge (0x05)";
        case MBM_ERR_EXCEP_SLAVE_DEVICE_BUSY:    return "Slave exception: Slave device busy (0x06)";
        case MBM_ERR_EXCEP_MEMORY_PARITY_ERROR:  return "Slave exception: Memory parity error (0x08)";
        case MBM_ERR_EXCEP_GATEWAY_PATH_UNAVAIL: return "Slave exception: Gateway path unavailable (0x0A)";
        case MBM_ERR_EXCEP_GATEWAY_TARGET_FAIL:  return "Slave exception: Gateway target device failed to respond (0x0B)";
        case MBM_ERR_EXCEP_UNKNOWN:              return "Slave exception: Unknown";
        default:                   return "Unknown";
    }
}

#endif // USE_MODBUS_MASTER
