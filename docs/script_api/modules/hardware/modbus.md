# Handle `modbus`

Modbus RTU master on one serial port. **Scripts can only read:** write functions are blocked so a script cannot move a machine.

Exists only when the firmware is built with `USE_MODBUS_MASTER`.

[Back to module `hardware`](README.md)

```c
import hardware as hw;

handle mb = hw.MODBUS();         // created closed
```

## Attributes

Read only. They hold the settings of the last successful `open()`, and read `0` before the first one.

| Attribute | Type | Meaning |
| :--- | :--- | :--- |
| `port` | `int` | Port number |
| `baud` | `int` | Baud rate |
| `frame` | `int` | `RTU_xxx` constant |

## Methods

| Method | Returns | Description |
| :--- | :--- | :--- |
| `open(int port, int baud, int frame)` | `string` | Open the port as master. `frame` is a `hw.RTU_xxx` constant |
| `request(int slave, int function, int address, int quantity, int timeout_ms)` | `dict` | Send one read and wait for the answer |
| `close()` | `string` | Close the port |

`open` and `close` return `"OK"` or the error text.

## `request`

| Function | Reads |
| :---: | :--- |
| `1` | Coils |
| `2` | Discrete inputs |
| `3` | Holding registers |
| `4` | Input registers |

It returns `{"status": string, "data": bytes}`. `status` is `"OK"` or the error text, and `data` is empty on error.

**Registers (functions 3, 4):** 2 bytes per register, high byte first.

```c
dict res = mb.request(2, 4, 1000, 1, 500);
string status = res["status"];
if (status == "OK") {
    bytes data = res["data"];
    int value = (data[0] << 8) | data[1];
    print(value);
} else {
    print_error("Read failed: ", status);
}
```

**Bits (functions 1, 2):** also 2 bytes per bit. Bit `i` is at `data[i * 2 + 1]`.

```c
bytes data = res["data"];
int third_bit = data[2 * 2 + 1];
```

The script waits until the answer arrives or `timeout_ms` passes. An answer holds at most 125 values.

## Errors

| Case | Result |
| :--- | :--- |
| Port, baud, frame, slave, address, quantity or timeout out of range | Raises |
| `function` is not `1..4` (every write included) | Raises, nothing is sent |
| `request` or `close` on a closed `modbus`, `open` on an already open one | Raises |
| No answer | `status` = `"Timeout"` |
| The answer is not valid | `status` = `"Invalid response"` |
| Quantity rejected by the library | `status` = `"Invalid request"` |
| Device refused the request | `status` = `"Slave exception: ..."`, for example `"Slave exception: Illegal data address (0x02)"` |
| Port in use | `"Port used by another owner"` or `"Port already open"` |
| Pins not configured | `"SERIAL<n>_TX / _RX pins not configured"` |

## Example

[`examples/hardware/modbus.ti`](../../../../examples/hardware/modbus.ti)
