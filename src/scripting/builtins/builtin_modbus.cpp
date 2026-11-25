/**
 * @file builtin_modbus.cpp
 * @brief Modbus master built-ins for TI scripts (see hardware/protocol/modbus_master.h).
 *
 *   modbus_open(port, baud, frame)   -> "OK" or the error text
 *   modbus_close(port)               -> "OK" or the error text
 *   modbus_is_open(port)             -> bool
 *   modbus_send(port, slave, function, address, quantity, timeout_ms) -> dict {"status", "data"}
 *
 * Two kinds of failure, reported like the serial_raw built-ins:
 *   - A mistake in the script (port out of range, baud <= 0, unknown frame name) is raised with
 *     ti_raise_error(): it cannot be right on a later run, the script must be fixed.
 *   - A state or hardware condition (port busy, already open, not open, no pins, stack failed) is returned
 *     as a string: "OK" on success, otherwise the text of ModbusMaster_StatusToStr().
 *
 * A port is opened ONCE and then used by every device (slave) on it.
 */
#include "config.h"
#include "modbus.h"
#include "portmacro.h"
#ifdef USE_MODBUS_MASTER
#include "built_in.h"
#include "TienInterpreter.h"
#include "hardware/protocol/modbus_master.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

/* -------------------------------------------------------------------------- */
/*                                   TYPES                                    */
/* -------------------------------------------------------------------------- */

typedef struct {
    /* Input passes to Modbus Send */
    uint8_t       port;
    uint8_t       slave;
    uint8_t       function;
    uint16_t      address;
    uint16_t      quantity;
    uint32_t      timeout_ms;

    /* Output passed to and recive from return of Modbus Send */
    uint8_t      *buf;
    size_t       *len;
    ModbusMasterStatus_t *status;

    // 3. Synchronize with ti script
    bool         *is_done;  // Atomic flag indicate task done
    ti_handle_t   handle;   // Script handle runtime
} ModbusAsyncRequest_t;

/* -------------------------------------------------------------------------- */
/*                                  Static                                    */
/* -------------------------------------------------------------------------- */

static QueueHandle_t s_modbus_queue[SERIAL_BUS_PORT_COUNT] = {NULL};     // A queue allow the main script wake up async Modbus task
static TaskHandle_t  s_modbus_worker[SERIAL_BUS_PORT_COUNT] = {NULL};    // Use to refrence, manage, interact with modbus worker
static void modbus_worker_task(void *arg)
{
    // Type cast arg to get the port_idx
    uint8_t port_idx = (uint8_t)(uintptr_t)arg;
    QueueHandle_t port_queue = s_modbus_queue[port_idx];
    ModbusAsyncRequest_t *req = NULL;
    for (;;)
    {
        // Wait on the queue that it manage
        if (xQueueReceive(port_queue, (void *)&req, portMAX_DELAY) == pdTRUE && req != NULL)
        {
            ModbusMasterStatus_t st = ModbusMaster_Send(req->port, req->slave, req->function,
                                                        req->address, req->quantity, req->timeout_ms,
                                                        req->buf, req->len);
            // Write result to status
            *(req->status) = st;

            // Turn on flag annouce finish the transaction
            __atomic_store_n(req->is_done, true, __ATOMIC_RELEASE);

            // Wake up the main script
            ti_give(req->handle);
        }
    }
}

/**
 * @brief Performs an asynchronous Modbus transaction using the worker queue.
 * Declared in built_in.h for use by modbus_send and hardware adapters.
 */
ModbusMasterStatus_t modbus_send_async(ti_handle_t handle,
                                       uint8_t port, uint8_t slave, uint8_t function,
                                       uint16_t address, uint16_t quantity, uint32_t timeout_ms,
                                       uint8_t *buf, size_t *len)
{
    uint8_t port_idx = port - 1;
    ModbusMasterStatus_t status = MBM_ERR_TIMEOUT;
    bool is_done = false;

    ModbusAsyncRequest_t req = {
        port, slave, function, address, quantity, timeout_ms,
        buf, len, &status, &is_done, handle
    };

    ModbusAsyncRequest_t *p_req = &req;
    xQueueSend(s_modbus_queue[port_idx], &p_req, portMAX_DELAY);

    ti_wait_flag(handle, &is_done, timeout_ms + 100);

    return status;
}

/**
 * @brief Starts the worker task and queue for the given port when opened.
 * @param port Port number (1-based).
 * @return True on success, false if queue or task creation failed.
 */
static bool modbus_worker_start(uint8_t port) {
    uint8_t port_idx = port - 1;
    if (port_idx >= SERIAL_BUS_PORT_COUNT) return false;

    if (s_modbus_queue[port_idx] == NULL) {
        s_modbus_queue[port_idx] = xQueueCreate(4, sizeof(ModbusAsyncRequest_t *));
        if (s_modbus_queue[port_idx] == NULL) {
            return false;
        }

        char task_name[16];
        snprintf(task_name, sizeof(task_name), "mb_worker_%d", (int)port);

        BaseType_t ret = xTaskCreatePinnedToCore(
            modbus_worker_task,
            task_name,
            4096,
            (void *)(uintptr_t)port_idx,
            configMAX_PRIORITIES - 3,
            &s_modbus_worker[port_idx],
            0 // Core 0
        );

        if (ret != pdPASS) {
            vQueueDelete(s_modbus_queue[port_idx]);
            s_modbus_queue[port_idx] = NULL;
            s_modbus_worker[port_idx] = NULL;
            return false;
        }
    }
    return true;
}
/**
 * @brief Check the port argument of a built-in; raise a script error when it is out of range.
 * @param handle Calling runtime handle.
 * @param fn Built-in name, used in the error message.
 * @param port Port number given by the script.
 * @return True when the port is valid; false after raising (the caller must return NULL).
 */
static bool checkPort(ti_handle_t handle, const char *fn, int port) {
    if (port >= 1 && port <= SERIAL_BUS_PORT_COUNT) return true;
    ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "%s: port must be 1..%d, got %d",
                   fn, SERIAL_BUS_PORT_COUNT, port);
    return false;
}

/**
 * @brief Built-in modbus_open(int port, int baud, string frame).
 * @param handle Calling runtime handle, used to report argument errors.
 * @param argv Arguments: (port, baud, frame). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return String "OK" or the error text; NULL after raising an error.
 */
static value_t *built_in_modbus_open(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    int port = argv[0]->int_val;
    int baud = argv[1]->int_val;
    const char *frame_str = argv[2]->string_val;

    // Step 1: script mistakes are raised (the port check also stops a wrong number wrapping in the uint8_t cast)
    if (!checkPort(handle, "modbus_open", port)) return NULL;
    if (baud <= 0) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "modbus_open: baud must be > 0, got %d", baud);
        return NULL;
    }

    // Step 2: frame name -> library constant
    ModbusSerialConfig_t frame;
    if (!ModbusMaster_FrameFromStr(frame_str, &frame)) {
        ti_raise_error(handle, TI_ERR_INVALID_ARG, "modbus_open: unknown frame '%s' (use e.g. \"8N1\", \"7E2\")",
                       frame_str);
        return NULL;
    }

    // Step 3: open and report the result as text
    ModbusMasterStatus_t status = ModbusMaster_Open((uint8_t)port, (uint32_t)baud, frame);
    if (status != MBM_OK) {
        return val_new_string(ModbusMaster_StatusToStr(status));
    }

    // Step 4: start async worker for this port; close port if worker fails to start
    if (!modbus_worker_start((uint8_t)port)) {
        ModbusMaster_Close((uint8_t)port);
        return val_new_string("Failed to start Modbus worker task");
    }

    return val_new_string("OK");
}

/**
 * @brief Built-in modbus_close(int port).
 * @param handle Calling runtime handle, used to report argument errors.
 * @param argv Arguments: (port). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return String "OK" or the error text; NULL after raising an error.
 */
static value_t *built_in_modbus_close(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    int port = argv[0]->int_val;

    // Step 1: a wrong port is a script mistake
    if (!checkPort(handle, "modbus_close", port)) return NULL;

    // Step 2: close and report the result as text
    return val_new_string(ModbusMaster_StatusToStr(ModbusMaster_Close((uint8_t)port)));
}

/**
 * @brief Built-in modbus_is_open(int port).
 * @param handle Calling runtime handle, used to report argument errors.
 * @param argv Arguments: (port). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Bool: true when the port is open; NULL after raising an error.
 */
static value_t *built_in_modbus_is_open(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    int port = argv[0]->int_val;

    // Step 1: a wrong port is a script mistake
    if (!checkPort(handle, "modbus_is_open", port)) return NULL;

    // Step 2: ask the protocol layer
    return val_new_bool(ModbusMaster_IsOpen((uint8_t)port));
}

/**
 * @brief Raise a script error when an integer argument does not fit its Modbus field.
 * @param handle Calling runtime handle.
 * @param name Argument name, used in the error message.
 * @param value Value given by the script.
 * @param max Largest allowed value (the smallest is 0).
 * @return True when 0 <= value <= max; false after raising (the caller must return NULL).
 */
static bool checkRange(ti_handle_t handle, const char *name, int value, int max) {
    if (value >= 0 && value <= max) return true;
    ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "modbus_send: %s must be 0..%d, got %d", name, max, value);
    return false;
}

/**
 * @brief Built-in modbus_send(int port, int slave, int function, int address, int quantity, int timeout_ms).
 * @param handle Calling runtime handle, used to report argument errors.
 * @param argv Arguments: (port, slave, function, address, quantity, timeout_ms). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Dict {"status": string, "data": bytes}; NULL after raising an error.
 */
static value_t *built_in_modbus_send(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    int port       = argv[0]->int_val;
    int slave      = argv[1]->int_val;
    int function   = argv[2]->int_val;
    int address    = argv[3]->int_val;
    int quantity   = argv[4]->int_val;
    int timeout_ms = argv[5]->int_val;

    // Step 1: script mistakes are raised (the range checks only stop a wrong number wrapping in the casts)
    if (!checkPort(handle, "modbus_send", port)) return NULL;
    if (!checkRange(handle, "slave", slave, 255)) return NULL;
    if (!checkRange(handle, "function", function, 255)) return NULL;
    if (!checkRange(handle, "address", address, 65535)) return NULL;
    if (!checkRange(handle, "quantity", quantity, 65535)) return NULL;
    if (timeout_ms <= 0) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "modbus_send: timeout_ms must be > 0, got %d", timeout_ms);
        return NULL;
    }

    // Step 2: send and wait for the answer asynchronously
    uint8_t buf[MODBUS_MASTER_MAX_RESPONSE];
    size_t len = sizeof(buf);
    ModbusMasterStatus_t status = modbus_send_async(handle, (uint8_t)port, (uint8_t)slave, (uint8_t)function,
                                                    (uint16_t)address, (uint16_t)quantity, (uint32_t)timeout_ms,
                                                    buf, &len);

    // Step 3: build {"status", "data"}; val_dict_set takes the value even when it fails
    value_t *bytes = val_new_bytes(buf, (int)len);
    value_t *result = val_new_dict();
    if (bytes == NULL || result == NULL) {
        val_free(bytes); val_free(result);
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "modbus_send: cannot allocate the result");
        return NULL;
    }
    ti_status_t st = val_dict_set(result->dict_val, "status", val_new_string(ModbusMaster_StatusToStr(status)));
    ti_status_t sd = val_dict_set(result->dict_val, "data", bytes);
    if (st != TI_OK || sd != TI_OK) {
        val_free(result);
        ti_raise_error(handle, (st != TI_OK) ? st : sd, "modbus_send: cannot build the result");
        return NULL;
    }
    return result;
}

void builtin_modbus_init(void) {
    static param_t send_params[]  = { { VAL_INT, (char *)"port" }, { VAL_INT, (char *)"slave" }, { VAL_INT, (char *)"function" },
                                      { VAL_INT, (char *)"address" }, { VAL_INT, (char *)"quantity" }, { VAL_INT, (char *)"timeout_ms" } };
    static param_t open_params[]  = { { VAL_INT, (char *)"port" }, { VAL_INT, (char *)"baud" }, { VAL_STRING, (char *)"frame" } };
    static param_t port_params[]  = { { VAL_INT, (char *)"port" } };
    register_builtin_function("modbus_open", VAL_STRING, open_params, 3, built_in_modbus_open);
    register_builtin_function("modbus_close", VAL_STRING, port_params, 1, built_in_modbus_close);
    register_builtin_function("modbus_is_open", VAL_BOOL, port_params, 1, built_in_modbus_is_open);
    register_builtin_function("modbus_send", VAL_DICT, send_params, 6, built_in_modbus_send);
}
#else //USE_MODBUS_MASTER
void builtin_modbus_init(void) {}
#endif //USE_MODBUS_MASTER
