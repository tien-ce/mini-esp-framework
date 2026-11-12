/**
 * @file modbus_handle.cpp
 * @brief Script handle "modbus": attributes, methods and destructor of modbus_handle_t.
 */

/*---- INCLUDES ----*/
#include "config.h"
#ifdef USE_MODBUS_MASTER
#include "scripting/handle/hardware/modbus_handle.h"

/*---- STATIC FUNCTION PROTOTYPES ----*/

static value_t *modbus_open(ti_handle_t handle, value_t **argv, int argc);
static value_t *modbus_request(ti_handle_t handle, value_t **argv, int argc);
static value_t *modbus_close(ti_handle_t handle, value_t **argv, int argc);
static ModbusMasterStatus_t modbus_close_port(modbus_handle_t *self);
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
 * @return Status of ModbusMaster_Close().
 */
static ModbusMasterStatus_t modbus_close_port(modbus_handle_t *self) {
    ModbusMasterStatus_t status = ModbusMaster_Close(self->port);
    self->is_open = false;
    return status;
}

/*---- STATIC FUNCTIONS: METHODS ----*/

/**
 * @brief Method modbus.open(int port, int baud, int frame): open the port as a Modbus RTU master.
 * @param handle Calling runtime handle, used to raise script mistakes.
 * @param argv Arguments: (self, port, baud, frame). frame is an RTU_xxx constant. Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return String "OK" or the error text of modbus_master; NULL after raising an error.
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

    // Step 2: values the uint8_t / uint32_t / enum casts would wrap (modbus_master does not check the frame)
    if (port < 1 || port > SERIAL_BUS_PORT_COUNT) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "modbus.open: port must be 1..%d, got %d",
                       SERIAL_BUS_PORT_COUNT, port);
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
    ModbusMasterStatus_t status = ModbusMaster_Open((uint8_t)port, (uint32_t)baud, (ModbusSerialConfig_t)frame);
    if (status == MBM_OK) {
        self->port    = (uint8_t)port;
        self->baud    = (uint32_t)baud;
        self->frame   = (ModbusSerialConfig_t)frame;
        self->is_open = true;
    }
    return val_new_string(ModbusMaster_StatusToStr(status));
}

/**
 * @brief Method modbus.request(slave, function, address, quantity, timeout_ms): send one request and wait for the answer.
 * @details For a write (function 5, 6), quantity carries the value to write. Slave, function and quantity are
 *          checked by the library (status MBM_ERR_REQUEST); here only the values the casts would wrap are raised.
 * @param handle Calling runtime handle, used to raise script mistakes.
 * @param argv Arguments: (self, slave, function, address, quantity, timeout_ms). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Dict {"status": "OK" or the error text of modbus_master, "data": bytes, 2 per register, high byte first
 *         (empty on error)}; NULL after raising an error.
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

    // Step 2: send and wait for the answer (blocks the script up to timeout_ms)
    uint8_t buf[MODBUS_MASTER_MAX_RESPONSE];
    size_t len = sizeof(buf);
    ModbusMasterStatus_t status = ModbusMaster_Send(self->port, (uint8_t)slave, (uint8_t)function, (uint16_t)address,
                                                    (uint16_t)quantity, (uint32_t)timeout_ms, buf, &len);

    // Step 3: build {"status", "data"}; val_dict_set takes the value even when it fails
    value_t *bytes  = val_new_bytes(buf, (int)len);
    value_t *result = val_new_dict();
    if (bytes == NULL || result == NULL) {
        val_free(bytes);
        val_free(result);
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "modbus.request: cannot allocate the result");
        return NULL;
    }
    ti_status_t st = val_dict_set(result->dict_val, "status", val_new_string(ModbusMaster_StatusToStr(status)));
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
 * @return String "OK" or the error text of modbus_master; NULL after raising an error.
 */
static value_t *modbus_close(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    modbus_handle_t *self = (modbus_handle_t *)argv[0]->handle_val->ptr;

    if (!self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "modbus.close: modbus is not open");
        return NULL;
    }
    return val_new_string(ModbusMaster_StatusToStr(modbus_close_port(self)));
}

/*---- STATIC FUNCTIONS: ATTRIBUTES ----*/

/**
 * @brief Read modbus.port, modbus.baud or modbus.frame (the settings given to the last successful open()).
 * @details Before the first open() all three read 0 (frame 0 is RTU_8N1).
 * @param ptr The modbus_handle_t (never NULL: a closed handle is rejected before the call).
 * @param name Attribute name written in the script.
 * @param out Receives a new int value owned by the caller (only on TI_OK).
 * @return TI_OK, TI_ERR_UNDEFINED for an unknown name, or TI_ERR_NO_MEMORY.
 */
static ti_status_t modbus_get_attr(void *ptr, const char *name, value_t **out) {
    const modbus_handle_t *self = (const modbus_handle_t *)ptr;
    int v;

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
 * @param ptr The modbus_handle_t (unused).
 * @param name Attribute name written in the script.
 * @param value New value (unused).
 * @return TI_ERR_READ_ONLY for port / baud / frame, TI_ERR_UNDEFINED for any other name.
 */
static ti_status_t modbus_set_attr(void *ptr, const char *name, const value_t *value) {
    (void)ptr;
    (void)value;

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
