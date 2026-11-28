/**
 * @file modbus_handle.cpp
 * @brief Script handle "modbus": attributes, methods and destructor of modbus_handle_t, and modbus_task, the task
 *        created for each request.
 *
 * Flow of request():
 *
 *     script task (native)                          modbus_task (one per request)
 *     --------------------                          -----------------------------
 *     fill self->slave, function, ..., runtime
 *     create modbus_task(self)           ----->     SerialModbus_Send, write status and data
 *     ti_wait_flag(&self->done)                     ti_signal_flag(runtime, &done): last access to self
 *       (signal handlers keep running)
 *     build {"status", "data"}           <-----     delete itself
 *
 * If the script is stopped while waiting, ti_wait_flag returns at once and the native keeps polling done, so it
 * only returns once modbus_task has left self.
 */

/*---- INCLUDES ----*/
#include "config.h"
#ifdef USE_MODBUS_MASTER
#include "scripting/handle/hardware/modbus_handle.h"
#include <freertos/task.h>

/*---- CONFIG ----*/

#define MODBUS_TASK_STACK_SIZE   4096    /* Bytes, stack of each modbus_task (the answer buffer is in self) */
#define MODBUS_TASK_PRIORITY     1
#define MODBUS_STOP_POLL_MS      10      /* Poll period for done when the script is stopped mid request */

/*---- DEFINES ----*/

/**
 * Function codes a script may send: the four reads (1 read coils, 2 read discrete inputs, 3 read holding
 * registers, 4 read input registers). Every other code is refused, not only the known writes (5, 6, 15, 16,
 * 22, 23): a write can move a machine, and a code this list does not know must not reach the device either.
 */
#define MODBUS_SCRIPT_FUNC_MIN  1
#define MODBUS_SCRIPT_FUNC_MAX  4

/*---- STATIC FUNCTION PROTOTYPES ----*/

static value_t *modbus_open(ti_handle_t handle, value_t **argv, int argc);
static value_t *modbus_request(ti_handle_t handle, value_t **argv, int argc);
static value_t *modbus_close(ti_handle_t handle, value_t **argv, int argc);
static SerialModbusStatus_t modbus_close_port(modbus_handle_t *self);
static void modbus_task(void *arg);
static void modbus_destroy(void *ptr);
static ti_status_t modbus_get_attr(void *ptr, const char *name, value_t **out);
static ti_status_t modbus_set_attr(void *ptr, const char *name, const value_t *value);

/*---- PARAMS ----*/

/** open(port, baud, frame). params[0] is self: the method is found through the handle's type, so it is always a modbus. */
static const param_t s_modbus_open_params[] = { PARAM_HANDLE("self", NULL), PARAM_INT("port"), PARAM_INT("baud"), PARAM_INT("frame"), };
/** request(slave, function, address, quantity, timeout_ms). */
static const param_t s_modbus_request_params[] = { PARAM_HANDLE("self", NULL), PARAM_INT("slave"), PARAM_INT("function"), PARAM_INT("address"), PARAM_INT("quantity"), PARAM_INT("timeout_ms"), };
/** close(). */
static const param_t s_modbus_self_params[] = { PARAM_HANDLE("self", NULL), };

/*---- METHODS ----*/

static const ti_builtin_def_t s_modbus_methods[] = {
    { "open",    VAL_STRING, s_modbus_open_params,    4, modbus_open    },
    { "request", VAL_DICT,   s_modbus_request_params, 6, modbus_request },
    { "close",   VAL_STRING, s_modbus_self_params,    1, modbus_close   },
};

/*---- HANDLE TYPE ----*/

/* External linkage comes from the extern declaration in modbus_handle.h */
const ti_handle_type_t g_modbus_handle_type = {
    "modbus",                                                       /* name */
    modbus_destroy,                                                 /* destroy */
    s_modbus_methods,                                               /* methods */
    (int)(sizeof(s_modbus_methods) / sizeof(s_modbus_methods[0])),  /* method_count */
    modbus_get_attr,                                                /* get_attr */
    modbus_set_attr,                                                /* set_attr */
};

/*---- STATIC FUNCTIONS: HELPERS ----*/

/**
 * @brief Close the port of an open modbus; the modbus object itself stays.
 * @param self An open modbus (is_open == true).
 * @return Status of SerialModbus_Close().
 */
static SerialModbusStatus_t modbus_close_port(modbus_handle_t *self) {
    SerialModbusStatus_t status = SerialModbus_Close(self->port);
    self->is_open = false;
    return status;
}

/*---- STATIC FUNCTIONS: MODBUS TASK ----*/

/**
 * @brief modbus_task: send the request stored in self, write the result into self, wake the script, then delete
 *        itself.
 * @details ti_signal_flag is the last access to self: the native may return as soon as done is set.
 * @param arg The modbus_handle_t* whose request fields were filled by the native.
 */
static void modbus_task(void *arg) {
    modbus_handle_t *self = (modbus_handle_t *)arg;

    self->data_len = sizeof(self->data);
    self->status   = SerialModbus_Send(self->port, self->slave, self->function, self->address, self->quantity,
                                       self->timeout_ms, self->data, &self->data_len);

    ti_signal_flag(self->runtime, &self->done);
    vTaskDelete(NULL);
}

/*---- STATIC FUNCTIONS: METHODS ----*/

/**
 * @brief Method modbus.open(int port, int baud, int frame): open the port as a Modbus RTU master.
 * @param handle Calling runtime handle, used to raise script mistakes.
 * @param argv Arguments: (self, port, baud, frame). frame is an RTU_xxx constant. Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return String "OK" or the error text of xdrv_04_serial_modbus; NULL after raising an error.
 */
static value_t *modbus_open(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    modbus_handle_t *self = (modbus_handle_t *)argv[0]->handle_val->ptr;
    int port  = argv[1]->int_val;
    int baud  = argv[2]->int_val;
    int frame = argv[3]->int_val;

    // Step 1: this handle already holds a port (close it first)
    if (self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "modbus.open: port %u is already open on this modbus", self->port);
        return NULL;
    }

    // Step 2: values the uint8_t / uint32_t / enum casts would wrap (the driver does not check the frame)
    if (port < 1 || port > SERIAL_MODBUS_PORT_MAX) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "modbus.open: port must be 1..%d, got %d",
                       SERIAL_MODBUS_PORT_MAX, port);
        return NULL;
    }
    if (baud <= 0) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "modbus.open: baud must be > 0, got %d", baud);
        return NULL;
    }
    if (frame < RTU_8N1 || frame > RTU_7O2) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "modbus.open: frame must be an RTU_xxx constant, got %d", frame);
        return NULL;
    }

    // Step 3: open, keep the settings only on success
    SerialModbusConfig_t cfg;
    cfg.baud  = (uint32_t)baud;
    cfg.frame = (ModbusSerialConfig_t)frame;
    SerialModbusStatus_t status = SerialModbus_Open((uint8_t)port, &cfg);
    if (status == SERIAL_MODBUS_OK) {
        self->port    = (uint8_t)port;
        self->baud    = cfg.baud;
        self->frame   = cfg.frame;
        self->is_open = true;
    }
    return val_new_string(SerialModbus_StatusToStr(status));
}

/**
 * @brief Method modbus.request(slave, function, address, quantity, timeout_ms): send one request and wait for the answer.
 * @details Only the read functions 1..4 are accepted: any other function (all the writes included) raises an
 *          error and nothing is sent, because writing to a device can move a machine. Slave and quantity are
 *          checked by the library (SERIAL_MODBUS_ERR_REQUEST); here only the values the casts would wrap are raised.
 *          The request runs in modbus_task; the script waits with ti_wait_flag, so its signal handlers keep running.
 * @param handle Calling runtime handle, used to raise script mistakes (also woken by modbus_task).
 * @param argv Arguments: (self, slave, function, address, quantity, timeout_ms). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Dict {"status": "OK" or the error text of the driver, "data": bytes, 2 per register, high byte first
 *         (empty on error)}; null when the script is being stopped; NULL after raising an error.
 */
static value_t *modbus_request(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    modbus_handle_t *self = (modbus_handle_t *)argv[0]->handle_val->ptr;
    int slave      = argv[1]->int_val;
    int function   = argv[2]->int_val;
    int address    = argv[3]->int_val;
    int quantity   = argv[4]->int_val;
    int timeout_ms = argv[5]->int_val;

    // Step 1: script mistakes are raised
    if (!self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "modbus.request: modbus is not open");
        return NULL;
    }
    if (slave < 0 || slave > 255 || function < 0 || function > 255) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "modbus.request: slave and function must be 0..255, got %d, %d",
                       slave, function);
        return NULL;
    }
    if (address < 0 || address > 65535 || quantity < 0 || quantity > 65535) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "modbus.request: address and quantity must be 0..65535, got %d, %d",
                       address, quantity);
        return NULL;
    }
    if (timeout_ms <= 0) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "modbus.request: timeout_ms must be > 0, got %d", timeout_ms);
        return NULL;
    }

    // Step 2: a script may only read; writing to a device can move a machine, so the request never leaves the board
    if (function < MODBUS_SCRIPT_FUNC_MIN || function > MODBUS_SCRIPT_FUNC_MAX) {
        ti_raise_error(handle, TI_ERR_INVALID_ARG,
                       "modbus.request: function 0x%02X is not allowed from a script (only reads 1..4; writes are blocked)",
                       function);
        return NULL;
    }

    // Step 3: one request at a time per handle (a signal handler may call request while the script waits)
    if (__atomic_exchange_n(&self->busy, true, __ATOMIC_ACQ_REL)) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "modbus.request: a request is already running on this modbus");
        return NULL;
    }

    // Step 4: copy the request into self and start the task that sends it
    self->slave      = (uint8_t)slave;
    self->function   = (uint8_t)function;
    self->address    = (uint16_t)address;
    self->quantity   = (uint16_t)quantity;
    self->timeout_ms = (uint32_t)timeout_ms;
    self->runtime    = handle;
    self->done       = false;

    if (xTaskCreate(modbus_task, "modbus_task", MODBUS_TASK_STACK_SIZE, self, MODBUS_TASK_PRIORITY, NULL) != pdPASS) {
        __atomic_store_n(&self->busy, false, __ATOMIC_RELEASE);
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "modbus.request: cannot start the request task");
        return NULL;
    }

    // Step 5: wait for done. No limit here: SerialModbus_Send always ends within timeout_ms
    if (ti_wait_flag(handle, &self->done, TI_PORT_WAIT_FOREVER) != TI_OK) {
        // Stopped: ti_wait_flag returns at once, so keep waiting here. modbus_task never touches self after
        // setting done, so self stays valid until this native returns
        while (!__atomic_load_n(&self->done, __ATOMIC_ACQUIRE)) {
            vTaskDelay(pdMS_TO_TICKS(MODBUS_STOP_POLL_MS));
        }
        __atomic_store_n(&self->busy, false, __ATOMIC_RELEASE);
        return val_new_null();
    }

    // Step 6: copy the result out of self, then free the handle for the next request
    value_t *status = val_new_string(SerialModbus_StatusToStr(self->status));
    value_t *bytes  = val_new_bytes(self->data, (int)self->data_len);
    __atomic_store_n(&self->busy, false, __ATOMIC_RELEASE);

    // Step 7: build {"status", "data"}; val_dict_set takes the value even when it fails
    value_t *result = val_new_dict();
    if (status == NULL || bytes == NULL || result == NULL) {
        val_free(status);
        val_free(bytes);
        val_free(result);
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "modbus.request: cannot allocate the result");
        return NULL;
    }
    ti_status_t st = val_dict_set(result->dict_val, "status", status);
    ti_status_t sd = val_dict_set(result->dict_val, "data", bytes);
    if (st != TI_OK || sd != TI_OK) {
        val_free(result);
        ti_raise_error(handle, (st != TI_OK) ? st : sd, "modbus.request: cannot build the result");
        return NULL;
    }
    return result;
}

/**
 * @brief Method modbus.close(): close the port. The modbus can be opened again.
 * @param handle Calling runtime handle, used to raise script mistakes.
 * @param argv Arguments: (self). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return String "OK" or the error text of xdrv_04_serial_modbus; NULL after raising an error.
 */
static value_t *modbus_close(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    modbus_handle_t *self = (modbus_handle_t *)argv[0]->handle_val->ptr;

    // A signal handler may call close while the script waits in request: modbus_task still uses the port
    if (__atomic_load_n(&self->busy, __ATOMIC_ACQUIRE)) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "modbus.close: a request is running on this modbus");
        return NULL;
    }
    if (!self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "modbus.close: modbus is not open");
        return NULL;
    }
    return val_new_string(SerialModbus_StatusToStr(modbus_close_port(self)));
}

/*---- STATIC FUNCTIONS: ATTRIBUTES ----*/

/**
 * @brief Read modbus.port, modbus.baud or modbus.frame (the settings given to the last successful open()).
 * @details Before the first open() all three read 0 (frame 0 is RTU_8N1). No attribute can be read while a
 *          request runs.
 * @param ptr The modbus_handle_t (never NULL: a closed handle is rejected before the call).
 * @param name Attribute name written in the script.
 * @param out Receives a new int value owned by the caller (only on TI_OK).
 * @return TI_OK, TI_ERR_INVALID_VALUE during a request, TI_ERR_UNDEFINED for an unknown name, or TI_ERR_NO_MEMORY.
 */
static ti_status_t modbus_get_attr(void *ptr, const char *name, value_t **out) {
    const modbus_handle_t *self = (const modbus_handle_t *)ptr;
    int v;

    if (__atomic_load_n(&self->busy, __ATOMIC_ACQUIRE)) return TI_ERR_INVALID_VALUE;

    if (strcmp(name, "port") == 0) {
        v = (int)self->port;
    } else if (strcmp(name, "baud") == 0) {
        v = (int)self->baud;
    } else if (strcmp(name, "frame") == 0) {
        v = (int)self->frame;
    } else {
        return TI_ERR_UNDEFINED;
    }

    *out = val_new_int(v);
    return (*out != NULL) ? TI_OK : TI_ERR_NO_MEMORY;
}

/**
 * @brief Write modbus.port / modbus.baud / modbus.frame: refused, the settings only change through open().
 * @details No attribute can be written while a request runs.
 * @param ptr The modbus_handle_t.
 * @param name Attribute name written in the script.
 * @param value New value (unused).
 * @return TI_ERR_INVALID_VALUE during a request, TI_ERR_READ_ONLY for port / baud / frame, TI_ERR_UNDEFINED for
 *         any other name.
 */
static ti_status_t modbus_set_attr(void *ptr, const char *name, const value_t *value) {
    const modbus_handle_t *self = (const modbus_handle_t *)ptr;
    (void)value;

    if (__atomic_load_n(&self->busy, __ATOMIC_ACQUIRE)) return TI_ERR_INVALID_VALUE;

    if (strcmp(name, "port") == 0 || strcmp(name, "baud") == 0 || strcmp(name, "frame") == 0) {
        return TI_ERR_READ_ONLY;
    }
    return TI_ERR_UNDEFINED;
}

/*---- STATIC FUNCTIONS: DESTRUCTOR ----*/

/**
 * @brief Destructor of the modbus handle type: runs when the last value pointing to the modbus is freed
 *        (out of scope, script ended or stopped). Closes the port if still open, then frees the object.
 * @param ptr The modbus_handle_t, allocated with calloc by MODBUS() (never NULL when called).
 */
static void modbus_destroy(void *ptr) {
    modbus_handle_t *self = (modbus_handle_t *)ptr;
    if (self->is_open) {
        modbus_close_port(self);
    }
    free(self);
}

#endif // USE_MODBUS_MASTER
