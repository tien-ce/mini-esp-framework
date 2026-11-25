# Serial Raw Built-ins

Plain bytes over a serial port (UART), for devices that do not need a dedicated protocol layer. Native module: `include/hardware/protocol/serial_raw.h`. Built-ins: `src/scripting/builtins/builtin_serial_raw.cpp`.

[Back to the index](../builtin_function_doc.md)

---

## Overview

```
UART hardware -> serial_bus (pins + ownership) -> serial_raw -> script built-ins / native drivers
```

- **Ports**: `1..N`, port `n` is UART `n` (`Serial1`, `Serial2`). `N` depends on the chip (`SOC_UART_NUM - 1`). The pins come from the pin configuration: `SERIAL<n>_TX`, `SERIAL<n>_RX` and the optional `SERIAL<n>_DE` (RS485 driver enable). Without TX/RX configured, `serial_raw_open` returns `"SERIAL<n>_TX / _RX pins not configured"`.
- **One user per port**: `serial_raw` registers on the bus with the owner name `"serial_raw"`. A port used by another module (e.g. Modbus) answers `"Port used by another owner"`, and a port already opened by `serial_raw` answers `"Port already open"`.
- **RS485**: when `SERIAL<n>_DE` is set, it is held LOW (receive) and driven HIGH only while sending; `serial_raw_write` returns after the last bit has left the UART.
- **Receive timing**: received bytes become visible as one finished burst, after the line was idle for 2 characters (or 120 bytes arrived). Slow senders that pause longer than that inside one frame are split into several bursts.

### Two kinds of failure

| Kind | When | What the script gets |
| :--- | :--- | :--- |
| Script mistake | port outside `1..N`, `baud <= 0`, unknown frame name, wrong `data` type, negative `max_len`, bad handler name | an **error is raised** (`TI_ERR_VALUE_OUT_OF_RANGE`, `TI_ERR_INVALID_ARG`, `TI_ERR_TYPE_MISMATCH`): the script stops with the kind, message and line |
| State / hardware | port busy, already open, not open, no pins, UART failed, empty data | a **string**: `"OK"` on success, otherwise the error text |

Error texts: `"Invalid serial port"`, `"Invalid argument"`, `"Port used by another owner"`, `"Port already open"`, `"SERIAL<n>_TX / _RX pins not configured"`, `"Port not open"`, `"UART failed to start"`.

---

## Functions

### `serial_raw_open(port, baud, frame)`
- **Prototype**: `string serial_raw_open(int port, int baud, string frame)`
- **Parameters**:
  - `port` (`int`): port number.
  - `baud` (`int`): bits per second, greater than 0.
  - `frame` (`string`): `"8N1"`, `"8N2"`, `"8E1"`, `"8E2"`, `"8O1"`, `"8O2"`, `"7N1"`, `"7N2"`, `"7E1"`, `"7E2"`, `"7O1"`, `"7O2"` (data bits, parity N/E/O, stop bits; case sensitive).
- **Return Value**: `"OK"` or the error text.

### `serial_raw_close(port)`
- **Prototype**: `string serial_raw_close(int port)`
- **Return Value**: `"OK"` or the error text (`"Port not open"` when it was never opened). Stops the UART, clears the receive handler and gives the port back to the bus.

### `serial_raw_write(port, data)`
- **Prototype**: `string serial_raw_write(int port, any data)`
- **Parameters**: `data` is a `string` (sent without the terminating `\0`) or `bytes`. Any other type raises `TI_ERR_TYPE_MISMATCH`.
- **Return Value**: `"OK"` or the error text. Empty `data` returns `"Invalid argument"`.

### `serial_raw_available(port)`
- **Prototype**: `int serial_raw_available(int port)`
- **Return Value**: number of received bytes waiting. `0` when nothing arrived, **or the port is not open** (use `serial_raw_read` to tell them apart).

### `serial_raw_read(port, max_len)`
- **Prototype**: `dict serial_raw_read(int port, int max_len)`
- **Return Value**: `{"status": string, "data": bytes}`.
  - `status`: `"OK"` or the error text (e.g. `"Port not open"`).
  - `data`: the bytes taken (at most `max_len`; the rest stays for the next call). Empty when nothing was waiting or on error.
- **Behavior**: never waits. `max_len` of `0` takes nothing and only reports the status.

### `serial_raw_on_receive(port, handler)`
- **Prototype**: `string serial_raw_on_receive(int port, func handler)`
- **Parameters**: `handler` is called as `handler(bytes data)` for every received burst, queued with `ti_post_event` (the script keeps running meanwhile).
- **Return Value**: `"OK"` or the error text (`"Port not open"` when the port is not open yet).
- **Behavior**:
  - While a handler is set, the received bytes go to it and are **not** left for `serial_raw_available` / `serial_raw_read`.
  - One handler per port; calling again replaces it. Closing the port removes it.
  - `data` is a `bytes` value with exactly the received bytes (`0x00` included). A burst longer than 256 bytes arrives as several calls.
  - If the registering script no longer exists, the handler is dropped silently.

---

## Examples

Polling:

```c
print(serial_raw_open(1, 9600, "8N1"));   // "OK" or the reason

serial_raw_write(1, "AT\r\n");
delay(100);

if (serial_raw_available(1) > 0) {
    dict res = serial_raw_read(1, 64);
    print(res["status"]);                 // "OK"; res["data"] holds the bytes
}
serial_raw_close(1);
```

Event driven:

```c
void on_data(bytes data) {
    print("got:", data);
}

print(serial_raw_open(2, 115200, "8N1"));
print(serial_raw_on_receive(2, on_data));
```

The examples show the call pattern; they have not been run on a board yet.
