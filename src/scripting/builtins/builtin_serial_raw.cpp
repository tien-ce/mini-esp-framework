/**
 * @file builtin_serial_raw.cpp
 * @brief Raw serial built-ins for TI scripts.
 *
 *   serial_raw_open(port, baud, frame)   -> "OK" or the error text
 *   serial_raw_close(port)               -> "OK" or the error text
 *   serial_raw_write(port, data)         -> "OK" or the error text (data: string or bytes)
 *   serial_raw_available(port)           -> int, bytes waiting (0 when nothing or the port is not open)
 *   serial_raw_read(port, max_len)       -> dict {"status": "OK" or the error text, "data": bytes}
 *   serial_raw_on_receive(port, handler) -> "OK" or the error text; handler(bytes data) runs per received burst
 *
 * Two kinds of failure, reported differently:
 *   - A mistake in the script (port out of range, baud <= 0, unknown frame name, data of a wrong type)
 *     is raised with ti_raise_error(): it cannot be right on a later run, the script must be fixed.
 *   - A state or hardware condition (port busy, already open, not open, no pins, UART failed, empty data)
 *     is returned as a string: "OK" on success, otherwise the text of SerialRaw_StatusToStr().
 *
 * The frame is given by name ("8N1", "7E2", ...) and converted by SerialRaw_FrameFromStr(), the same
 * way any built-in that needs an Arduino constant should do it.
 */

#include "built_in.h"
#include "TienInterpreter.h"
#include "hardware/protocol/serial_raw.h"
#include <Arduino.h>
#include <string.h>
#include <mutex>

#define HANDLER_NAME_MAX    32      /* Longest handler function name (including the terminator) */

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
 * @brief Built-in serial_raw_open(int port, int baud, string frame).
 * @param handle Calling runtime handle, used to report argument errors.
 * @param argv Arguments: (port, baud, frame). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return String "OK" or the error text; NULL after raising an error.
 */
static value_t *built_in_serial_raw_open(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    int port = argv[0]->int_val;
    int baud = argv[1]->int_val;
    const char *frame_str = argv[2]->string_val;

    // Step 1: script mistakes are raised (the port check also stops a wrong number wrapping in the uint8_t cast)
    if (!checkPort(handle, "serial_raw_open", port)) return NULL;
    if (baud <= 0) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "serial_raw_open: baud must be > 0, got %d", baud);
        return NULL;
    }

    // Step 2: frame name -> Arduino constant
    SerialRawConfig_t cfg;
    cfg.baud = (uint32_t)baud;
    if (!SerialRaw_FrameFromStr(frame_str, &cfg.frame)) {
        ti_raise_error(handle, TI_ERR_INVALID_ARG, "serial_raw_open: unknown frame '%s' (use e.g. \"8N1\", \"7E2\")",
                       frame_str);
        return NULL;
    }

    // Step 3: open and report the result as text
    return val_new_string(SerialRaw_StatusToStr(SerialRaw_Open((uint8_t)port, &cfg)));
}

/**
 * @brief Built-in serial_raw_close(int port).
 * @param handle Calling runtime handle, used to report argument errors.
 * @param argv Arguments: (port). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return String "OK" or the error text; NULL after raising an error.
 */
static value_t *built_in_serial_raw_close(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    int port = argv[0]->int_val;

    // Step 1: a wrong port is a script mistake
    if (!checkPort(handle, "serial_raw_close", port)) return NULL;

    // Step 2: close and report the result as text
    return val_new_string(SerialRaw_StatusToStr(SerialRaw_Close((uint8_t)port)));
}

/**
 * @brief Built-in serial_raw_write(int port, any data): data is a string or bytes.
 * @param handle Calling runtime handle, used to report argument errors.
 * @param argv Arguments: (port, data). Owned by the interpreter, only read, never kept.
 * @param argc Argument count (unused).
 * @return String "OK" or the error text; NULL after raising an error.
 */
static value_t *built_in_serial_raw_write(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    int port = argv[0]->int_val;
    const value_t *data = argv[1];

    // Step 1: a wrong port is a script mistake
    if (!checkPort(handle, "serial_raw_write", port)) return NULL;

    // Step 2: VAL_ANY accepts every type, so only string and bytes are taken here
    const uint8_t *buf = NULL;
    size_t len = 0;
    if (data->type == VAL_STRING) {
        buf = (const uint8_t *)data->string_val;
        len = strlen(data->string_val);   // the terminating '\0' is not sent
    } else if (data->type == VAL_BYTES) {
        buf = val_bytes_data(data);
        len = val_bytes_len(data);
    } else {
        ti_raise_error(handle, TI_ERR_TYPE_MISMATCH, "serial_raw_write: 'data' must be string or bytes");
        return NULL;
    }

    // Step 3: send and report the result as text (empty data comes back as "Invalid argument")
    return val_new_string(SerialRaw_StatusToStr(SerialRaw_Write((uint8_t)port, buf, len)));
}

/**
 * @brief Built-in serial_raw_available(int port).
 * @param handle Calling runtime handle, used to report argument errors.
 * @param argv Arguments: (port). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Number of received bytes waiting (0 when the port is not open); NULL after raising an error.
 */
static value_t *built_in_serial_raw_available(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    int port = argv[0]->int_val;

    // Step 1: a wrong port is a script mistake
    if (!checkPort(handle, "serial_raw_available", port)) return NULL;

    // Step 2: count the waiting bytes
    return val_new_int((int)SerialRaw_Available((uint8_t)port));
}

/**
 * @brief Built-in serial_raw_read(int port, int max_len): returns {"status": string, "data": bytes}.
 * @details "status" is "OK" or the error text. "data" holds the bytes taken (empty when nothing was waiting
 *          or on error). At most max_len bytes are taken, the rest stays for the next call. Never waits.
 * @param handle Calling runtime handle, used to report argument errors.
 * @param argv Arguments: (port, max_len). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return New dict; NULL after raising an error.
 */
static value_t *built_in_serial_raw_read(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    int port = argv[0]->int_val;
    int max_len = argv[1]->int_val;

    // Step 1: script mistakes are raised
    if (!checkPort(handle, "serial_raw_read", port)) return NULL;
    if (max_len < 0) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "serial_raw_read: max_len must be >= 0, got %d", max_len);
        return NULL;
    }

    // Step 2: size the result to what is waiting, so the bytes are read straight into it
    size_t count = SerialRaw_Available((uint8_t)port);
    if (count > (size_t)max_len) count = (size_t)max_len;
    if (count > TI_MAX_BYTES_LEN) count = TI_MAX_BYTES_LEN;

    uint8_t *dst = NULL;
    value_t *bytes = val_new_bytes_alloc((int)count, &dst);
    value_t *result = val_new_dict();
    if (bytes == NULL || result == NULL) {
        val_free(bytes); val_free(result);
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "serial_raw_read: cannot allocate the result");
        return NULL;
    }

    // Step 3: take the bytes (count 0 only reports the status, e.g. a closed port)
    size_t got = 0;
    SerialRawStatus_t status = SerialRaw_Read((uint8_t)port, dst, count, &got);

    // Step 4: build {"status", "data"}; val_dict_set takes the value even when it fails
    ti_status_t st = val_dict_set(result->dict_val, "status", val_new_string(SerialRaw_StatusToStr(status)));
    ti_status_t sd = val_dict_set(result->dict_val, "data", bytes);
    if (st != TI_OK || sd != TI_OK) {
        val_free(result);
        ti_raise_error(handle, (st != TI_OK) ? st : sd, "serial_raw_read: cannot build the result");
        return NULL;
    }
    return result;
}

/** Script handler of one port, filled by serial_raw_on_receive(). */
typedef struct {
    bool        used;                 /* True when a handler is registered */
    ti_handle_t handle;               /* Runtime that registered it */
    char        name[HANDLER_NAME_MAX]; /* Handler function name (copy) */
} RxHandler_t;

static RxHandler_t s_rx[SERIAL_BUS_PORT_COUNT];   /* Index = port - 1, guarded by s_rx_lock */
static std::mutex  s_rx_lock;

/**
 * @brief serial_raw RX callback: post the received bytes to the script handler of the port as a bytes value.
 * @details Runs in the UART event task. The bytes value holds exactly len bytes (0x00 included).
 * @param port Port that received data.
 * @param data Received bytes.
 * @param len Number of bytes.
 */
static void onSerialRx(uint8_t port, const uint8_t *data, size_t len) {
    // Step 1: copy the handler out, the lock must not be held while calling into the script runtime
    RxHandler_t h;
    {
        std::lock_guard<std::mutex> guard(s_rx_lock);
        h = s_rx[port - 1];
    }
    if (!h.used) return;

    // Step 2: exactly len bytes -> VAL_BYTES (ti_post_event takes it, also on failure)
    value_t *arg = val_new_bytes(data, (int)len);
    if (arg == NULL) return;
    value_t *args[1] = { arg };

    // Step 3: a script that is gone cannot receive any more: drop its handler
    if (ti_post_event(h.handle, h.name, args, 1) == TI_ERR_STALE_HANDLE) {
        std::lock_guard<std::mutex> guard(s_rx_lock);
        s_rx[port - 1].used = false;
        SerialRaw_SetRxCallback(port, NULL);
    }
}

/**
 * @brief Built-in serial_raw_on_receive(int port, func handler).
 * @details The handler is called as handler(bytes data) for every burst received on the port. While it is
 *          set, serial_raw_available() / serial_raw_read() see nothing on that port.
 * @param handle Calling runtime handle (stored so the data can reach the script later).
 * @param argv Arguments: (port, handler). Owned by the interpreter, never kept.
 * @param argc Argument count (unused).
 * @return String "OK" or the error text; NULL after raising an error.
 */
static value_t *built_in_serial_raw_on_receive(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    int port = argv[0]->int_val;
    const char *name = argv[1]->func_name;

    // Step 1: script mistakes are raised
    if (!checkPort(handle, "serial_raw_on_receive", port)) return NULL;
    if (name == NULL || name[0] == '\0' || strlen(name) >= HANDLER_NAME_MAX) {
        ti_raise_error(handle, TI_ERR_INVALID_ARG, "serial_raw_on_receive: handler name must be 1..%d characters",
                       HANDLER_NAME_MAX - 1);
        return NULL;
    }

    // Step 2: store the handler (a COPY of the name), then ask serial_raw to call us
    {
        std::lock_guard<std::mutex> guard(s_rx_lock);
        RxHandler_t *h = &s_rx[port - 1];
        h->handle = handle;
        strncpy(h->name, name, HANDLER_NAME_MAX - 1);
        h->name[HANDLER_NAME_MAX - 1] = '\0';
        h->used = true;
    }
    SerialRawStatus_t status = SerialRaw_SetRxCallback((uint8_t)port, onSerialRx);

    // Step 3: forget the handler when serial_raw refused (e.g. port not open)
    if (status != SERIAL_RAW_OK) {
        std::lock_guard<std::mutex> guard(s_rx_lock);
        s_rx[port - 1].used = false;
    }
    return val_new_string(SerialRaw_StatusToStr(status));
}

void builtin_serial_raw_init(void) {
    static param_t open_params[]  = { { VAL_INT, (char *)"port" }, { VAL_INT, (char *)"baud" }, { VAL_STRING, (char *)"frame" } };
    static param_t close_params[] = { { VAL_INT, (char *)"port" } };
    static param_t write_params[] = { { VAL_INT, (char *)"port" }, { VAL_ANY, (char *)"data" } };
    static param_t avail_params[] = { { VAL_INT, (char *)"port" } };
    static param_t read_params[]  = { { VAL_INT, (char *)"port" }, { VAL_INT, (char *)"max_len" } };
    static param_t recv_params[]  = { { VAL_INT, (char *)"port" }, { VAL_FUNC, (char *)"handler", VAL_NULL } };
    register_builtin_function("serial_raw_on_receive", VAL_STRING, recv_params, 2, built_in_serial_raw_on_receive);
    register_builtin_function("serial_raw_available", VAL_INT, avail_params, 1, built_in_serial_raw_available);
    register_builtin_function("serial_raw_read", VAL_DICT, read_params, 2, built_in_serial_raw_read);
    register_builtin_function("serial_raw_open", VAL_STRING, open_params, 3, built_in_serial_raw_open);
    register_builtin_function("serial_raw_close", VAL_STRING, close_params, 1, built_in_serial_raw_close);
    register_builtin_function("serial_raw_write", VAL_STRING, write_params, 2, built_in_serial_raw_write);
}
