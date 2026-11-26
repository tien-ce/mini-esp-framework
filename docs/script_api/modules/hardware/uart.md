# Handle `uart`

Raw bytes on one serial port.

[Back to module `hardware`](README.md)

```c
import hardware as hw;

handle uart = hw.UART();         // created closed
```

## Attributes

Read only. They hold the settings of the last successful `open()`, and read `0` before the first one.

| Attribute | Type | Meaning |
| :--- | :--- | :--- |
| `port` | `int` | Port number |
| `baud` | `int` | Baud rate |
| `frame` | `int` | `SERIAL_xxx` constant |

## Methods

| Method | Returns | Description |
| :--- | :--- | :--- |
| `open(int port, int baud, int frame)` | `string` | Open the port. `frame` is a `hw.SERIAL_xxx` constant |
| `write(data)` | `string` | Send a `string` or `bytes`. Returns after the last bit has left the UART |
| `is_available()` | `bool` | `true` when received bytes are waiting |
| `read(int max_len)` | `bytes` | Take up to `max_len` waiting bytes. Never waits, empty when nothing is there |
| `on_receive(func handler)` | `string` | Call `handler(bytes data)` when data arrives |
| `close()` | `string` | Close the port and remove the handler |

Methods returning `string` give `"OK"` or the error text.

## Receiving data

**Polling:**

```c
if (uart.is_available()) {
    bytes data = uart.read(64);
    print(bytes_to_string(data));
}
```

**With a handler:**

```c
void on_data(bytes data) {
    print(bytes_to_string(data));
}

uart.on_receive(on_data);
while (true) {
    delay(1000);                 // the handler runs while the script waits
}
```

- Bytes appear as one burst after the line is idle for 2 characters, or after 120 bytes. A sender that pauses inside a frame is split into several bursts.
- A burst longer than 256 bytes arrives as several handler calls.
- While a handler is set, `is_available()` and `read()` see nothing.
- A port has one handler. Calling `on_receive` again replaces it.
- Handler calls are queued, up to 32 per script. When the queue is full, new data is dropped.

## Errors

| Case | Result |
| :--- | :--- |
| Port outside the valid range, `baud <= 0`, negative `max_len` | Raises |
| `data` is not `string` or `bytes`, handler is an empty function | Raises |
| Method called on a closed `uart`, `open()` on an already open `uart` | Raises |
| Empty `data` | `"Invalid argument"` |
| Port in use | `"Port used by another owner"` or `"Port already open"` |
| Pins not configured | `"SERIAL<n>_TX / _RX pins not configured"` |
| UART cannot start | `"UART failed to start"` |

## Example

[`examples/hardware/uart.ti`](../../../../examples/hardware/uart.ti)
