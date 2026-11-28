/**
 * @file uart_handle.cpp
 * @brief Script handle "uart": attributes, methods and destructor of uart_handle_t.
 */

/*---- INCLUDES ----*/
#include "scripting/handle/hardware/uart_handle.h"

/*---- STATIC FUNCTION PROTOTYPES ----*/

static void uart_on_rx(uint8_t port, const uint8_t *data, size_t len, void *arg);
static SerialUartStatus_t uart_close_port(uart_handle_t *self);
static void uart_destroy(void *ptr);

static value_t *uart_open(ti_handle_t handle, value_t **argv, int argc);
static value_t *uart_write(ti_handle_t handle, value_t **argv, int argc);
static value_t *uart_is_available(ti_handle_t handle, value_t **argv, int argc);
static value_t *uart_read(ti_handle_t handle, value_t **argv, int argc);
static value_t *uart_on_receive(ti_handle_t handle, value_t **argv, int argc);
static value_t *uart_close(ti_handle_t handle, value_t **argv, int argc);
static ti_status_t uart_get_attr(void *ptr, const char *name, value_t **out);
static ti_status_t uart_set_attr(void *ptr, const char *name, const value_t *value);

/*---- PARAMS ----*/

/** open(port, baud, frame). params[0] is self: the method is found through the handle's type, so it is always a uart. */
static const param_t s_uart_open_params[] = { PARAM_HANDLE("self", NULL), PARAM_INT("port"), PARAM_INT("baud"), PARAM_INT("frame"), };
/** write(data). data is string or bytes, checked inside the method (VAL_ANY). */
static const param_t s_uart_write_params[] = { PARAM_HANDLE("self", NULL), PARAM_ANY("data"), };
/** is_available(), close(). */
static const param_t s_uart_self_params[] = { PARAM_HANDLE("self", NULL), };
/** read(max_len). */
static const param_t s_uart_read_params[] = { PARAM_HANDLE("self", NULL), PARAM_INT("max_len"), };
/** on_receive(handler). */
static const param_t s_uart_on_receive_params[] = { PARAM_HANDLE("self", NULL), PARAM_FUNC("handler"), };

/*---- METHODS ----*/

static const ti_builtin_def_t s_uart_methods[] = {
    { "open",         VAL_STRING, s_uart_open_params,       4, uart_open         },
    { "write",        VAL_STRING, s_uart_write_params,      2, uart_write        },
    { "is_available", VAL_BOOL,   s_uart_self_params,       1, uart_is_available },
    { "read",         VAL_BYTES,  s_uart_read_params,       2, uart_read         },
    { "on_receive",   VAL_STRING, s_uart_on_receive_params, 2, uart_on_receive   },
    { "close",        VAL_STRING, s_uart_self_params,       1, uart_close        },
};

/*---- HANDLE TYPE ----*/

/* External linkage comes from the extern declaration in uart_handle.h */
const ti_handle_type_t g_uart_handle_type = {
    "uart",                                                         /* name */
    uart_destroy,                                                   /* destroy */
    s_uart_methods,                                                 /* methods */
    (int)(sizeof(s_uart_methods) / sizeof(s_uart_methods[0])),      /* method_count */
    uart_get_attr,                                                  /* get_attr */
    uart_set_attr,                                                  /* set_attr */
};

/*---- STATIC FUNCTIONS: SERIAL UART CALLBACK ----*/

/**
 * @brief Serial UART RX callback of every uart: post the received bytes to the handler of the uart that set it.
 * @details Runs in the UART event task with the serial UART port lock held: never call a SerialUart_ function here.
 *          The handle is stable: on_receive() and close remove this callback (same lock) before changing or
 *          freeing it.
 * @param port Port that received data (unused: arg already is the handle).
 * @param data Received bytes (valid only during the call).
 * @param len  Number of bytes.
 * @param arg  The uart_handle_t given to SerialUart_SetRxCallback.
 */
static void uart_on_rx(uint8_t port, const uint8_t *data, size_t len, void *arg) {
    (void)port;
    const uart_handle_t *self = (const uart_handle_t *)arg;

    // Exactly len bytes -> VAL_BYTES (ti_post_event takes it, also on failure; the function value is borrowed)
    value_t *bytes = val_new_bytes(data, (int)len);
    if (bytes == NULL) return;
    value_t *args[1] = { bytes };
    ti_post_event(self->runtime, self->on_receive, args, 1);
}

/*---- STATIC FUNCTIONS: HELPERS ----*/

/**
 * @brief Close the port of an open uart and drop its handler; the uart object itself stays.
 * @details Order matters: SerialUart_Close() waits for a running callback and removes it, so the handler is
 *          freed only when no callback can read it any more.
 * @param self An open uart (is_open == true).
 * @return Status of SerialUart_Close().
 */
static SerialUartStatus_t uart_close_port(uart_handle_t *self) {
    SerialUartStatus_t status = SerialUart_Close(self->port);
    val_free(self->on_receive);   // val_free(NULL) does nothing
    self->on_receive = NULL;
    self->runtime    = TI_INVALID_HANDLE;
    self->is_open    = false;
    return status;
}

/*---- STATIC FUNCTIONS: METHODS ----*/

/**
 * @brief Method uart.open(int port, int baud, int frame): open the port in raw mode.
 * @param handle Calling runtime handle, used to raise script mistakes.
 * @param argv Arguments: (self, port, baud, frame). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return String "OK" or the error text of xdrv_03_serial_uart; NULL after raising an error.
 */
static value_t *uart_open(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    uart_handle_t *self = (uart_handle_t *)argv[0]->handle_val->ptr;
    int port  = argv[1]->int_val;
    int baud  = argv[2]->int_val;
    int frame = argv[3]->int_val;

    // Step 1: this handle already holds a port (close it first)
    if (self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "uart.open: port %u is already open on this uart", self->port);
        return NULL;
    }

    // Step 2: values the uint8_t / uint32_t casts would wrap
    if (port < 1 || port > SERIAL_UART_PORT_MAX) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "uart.open: port must be 1..%d, got %d",
                       SERIAL_UART_PORT_MAX, port);
        return NULL;
    }
    if (baud <= 0) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "uart.open: baud must be > 0, got %d", baud);
        return NULL;
    }

    // Step 3: open, keep the settings only on success
    SerialUartConfig_t cfg;
    cfg.baud  = (uint32_t)baud;
    cfg.frame = (uint32_t)frame;
    SerialUartStatus_t status = SerialUart_Open((uint8_t)port, &cfg);
    if (status == SERIAL_UART_OK) {
        self->port    = (uint8_t)port;
        self->cfg     = cfg;
        self->is_open = true;
    }
    return val_new_string(SerialUart_StatusToStr(status));
}

/**
 * @brief Method uart.write(string|bytes data): send the bytes on the port of this uart.
 * @param handle Calling runtime handle, used to raise script mistakes.
 * @param argv Arguments: (self, data). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return String "OK" or the error text of xdrv_03_serial_uart; NULL after raising an error.
 */
static value_t *uart_write(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    uart_handle_t *self = (uart_handle_t *)argv[0]->handle_val->ptr;
    const value_t *data = argv[1];

    // Step 1: VAL_ANY accepts every type, so only string and bytes are taken here
    const uint8_t *buf = NULL;
    size_t len = 0;
    if (data->type == VAL_STRING) {
        buf = (const uint8_t *)data->string_val;
        len = strlen(data->string_val);   // the terminating '\0' is not sent
    } else if (data->type == VAL_BYTES) {
        buf = val_bytes_data(data);
        len = val_bytes_len(data);
    } else {
        ti_raise_error(handle, TI_ERR_TYPE_MISMATCH, "uart.write: 'data' must be string or bytes");
        return NULL;
    }

    // Step 2: only the port this uart opened (self->port may belong to another uart otherwise)
    if (!self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "uart.write: uart is not open");
        return NULL;
    }

    // Step 3: send (empty data comes back as "Invalid argument")
    return val_new_string(SerialUart_StatusToStr(SerialUart_Write(self->port, buf, len)));
}

/**
 * @brief Method uart.is_available(): true when received bytes are waiting to be read.
 * @param handle Calling runtime handle, used to raise script mistakes.
 * @param argv Arguments: (self). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Bool value; NULL after raising an error.
 */
static value_t *uart_is_available(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    uart_handle_t *self = (uart_handle_t *)argv[0]->handle_val->ptr;

    if (!self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "uart.is_available: uart is not open");
        return NULL;
    }
    return val_new_bool(SerialUart_Available(self->port) > 0);
}

/**
 * @brief Method uart.read(int max_len): take up to max_len received bytes. Never waits.
 * @param handle Calling runtime handle, used to raise script mistakes.
 * @param argv Arguments: (self, max_len). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Bytes value (empty when nothing is waiting); NULL after raising an error.
 */
static value_t *uart_read(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    uart_handle_t *self = (uart_handle_t *)argv[0]->handle_val->ptr;
    int max_len = argv[1]->int_val;

    // Step 1: script mistakes are raised
    if (!self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "uart.read: uart is not open");
        return NULL;
    }
    if (max_len < 0) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "uart.read: max_len must be >= 0, got %d", max_len);
        return NULL;
    }

    // Step 2: size the result to what is waiting, so the bytes are read straight into it
    size_t count = SerialUart_Available(self->port);
    if (count > (size_t)max_len) count = (size_t)max_len;
    if (count > TI_MAX_BYTES_LEN) count = TI_MAX_BYTES_LEN;

    uint8_t *dst = NULL;
    value_t *bytes = val_new_bytes_alloc((int)count, &dst);
    if (bytes == NULL) {
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "uart.read: cannot allocate %u bytes", (unsigned)count);
        return NULL;
    }

    // Step 3: take the bytes (only more can arrive after Available(), so all count bytes are there)
    size_t got = 0;
    SerialUartStatus_t status = SerialUart_Read(self->port, dst, count, &got);
    if (status != SERIAL_UART_OK) {
        val_free(bytes);
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "uart.read: %s", SerialUart_StatusToStr(status));
        return NULL;
    }
    return bytes;
}

/**
 * @brief Method uart.on_receive(func handler): call handler(bytes data) for every burst received on the port.
 * @details Setting it again replaces the handler. The calling runtime is stored so the data reaches this script.
 * @param handle Calling runtime handle (stored in the uart, also used to raise script mistakes).
 * @param argv Arguments: (self, handler). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return String "OK" or the error text of xdrv_03_serial_uart; NULL after raising an error.
 */
static value_t *uart_on_receive(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    uart_handle_t *self = (uart_handle_t *)argv[0]->handle_val->ptr;

    // Step 1: script mistakes are raised (xdrv_03_serial_uart takes a callback only on an open port)
    if (!self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "uart.on_receive: uart is not open");
        return NULL;
    }
    if (val_func_name(argv[1]) == NULL) {
        ti_raise_error(handle, TI_ERR_INVALID_ARG, "uart.on_receive: expects a defined function");
        return NULL;
    }

    // Step 2: own copy of the handler (argv is freed after this call)
    value_t *func = val_copy(argv[1]);
    if (func == NULL) {
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "uart.on_receive: out of memory");
        return NULL;
    }

    // Step 3: remove the callback first. SerialUart_SetRxCallback takes the port lock that a running callback holds,
    // so after it returns no callback reads self and the old handler can be freed
    SerialUart_SetRxCallback(self->port, NULL, NULL);
    val_free(self->on_receive);
    self->on_receive = func;
    self->runtime    = handle;

    // Step 4: register the callback again, with this uart as arg so the callback finds its handler
    return val_new_string(SerialUart_StatusToStr(SerialUart_SetRxCallback(self->port, uart_on_rx, self)));
}

/**
 * @brief Method uart.close(): close the port and drop the receive handler. The uart can be opened again.
 * @param handle Calling runtime handle, used to raise script mistakes.
 * @param argv Arguments: (self). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return String "OK" or the error text of xdrv_03_serial_uart; NULL after raising an error.
 */
static value_t *uart_close(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    uart_handle_t *self = (uart_handle_t *)argv[0]->handle_val->ptr;

    if (!self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "uart.close: uart is not open");
        return NULL;
    }
    return val_new_string(SerialUart_StatusToStr(uart_close_port(self)));
}

/*---- STATIC FUNCTIONS: ATTRIBUTES ----*/

/**
 * @brief Read uart.port, uart.baud or uart.frame (the settings given to the last successful open()).
 * @details Before the first open() all three read 0.
 * @param ptr The uart_handle_t (never NULL: a closed handle is rejected before the call).
 * @param name Attribute name written in the script.
 * @param out Receives a new int value owned by the caller (only on TI_OK).
 * @return TI_OK, TI_ERR_UNDEFINED for an unknown name, or TI_ERR_NO_MEMORY.
 */
static ti_status_t uart_get_attr(void *ptr, const char *name, value_t **out) {
    const uart_handle_t *self = (const uart_handle_t *)ptr;
    int v;

    if (strcmp(name, "port") == 0) {
        v = (int)self->port;
    } else if (strcmp(name, "baud") == 0) {
        v = (int)self->cfg.baud;
    } else if (strcmp(name, "frame") == 0) {
        v = (int)self->cfg.frame;
    } else {
        return TI_ERR_UNDEFINED;
    }

    *out = val_new_int(v);
    return (*out != NULL) ? TI_OK : TI_ERR_NO_MEMORY;
}

/**
 * @brief Write uart.port / uart.baud / uart.frame: refused, the settings only change through open().
 * @param ptr The uart_handle_t (unused).
 * @param name Attribute name written in the script.
 * @param value New value (unused).
 * @return TI_ERR_READ_ONLY for port / baud / frame, TI_ERR_UNDEFINED for any other name.
 */
static ti_status_t uart_set_attr(void *ptr, const char *name, const value_t *value) {
    (void)ptr;
    (void)value;

    if (strcmp(name, "port") == 0 || strcmp(name, "baud") == 0 || strcmp(name, "frame") == 0) {
        return TI_ERR_READ_ONLY;
    }
    return TI_ERR_UNDEFINED;
}

/*---- STATIC FUNCTIONS: DESTRUCTOR ----*/

/**
 * @brief Destructor of the uart handle type: runs when the last value pointing to the uart is freed
 *        (out of scope, script ended or stopped). Closes the port if still open, then frees the object.
 * @param ptr The uart_handle_t, allocated with calloc by UART() (never NULL when called).
 */
static void uart_destroy(void *ptr) {
    uart_handle_t *self = (uart_handle_t *)ptr;
    if (self->is_open) {
        uart_close_port(self);
    }
    free(self);
}
