# Module `hardware`

Ports and outputs of the board.

[Back to the API index](../../README.md)

```c
import hardware as hw;
```

## Handles

| Create with | Handle | Use it for | Needs build flag |
| :--- | :--- | :--- | :--- |
| `hw.UART()` | [`uart`](uart.md) | Raw bytes on a serial port | none |
| `hw.MODBUS()` | [`modbus`](modbus.md) | Modbus RTU master (read only) | `USE_MODBUS_MASTER` |

## Functions

Relays are numbered `1..3`. These functions exist only when the firmware is built with `USE_RELAY`.

| Function | Returns | Description |
| :--- | :--- | :--- |
| `relay_get(int relay)` | `bool` | `true` when the relay is on |
| `relay_set(int relay, state)` | void | `state`: `bool`, `int` (0 = off, other = on), `"ON"` or `"OFF"` |
| `relay_toggle(int relay)` | void | Switch to the opposite state |

A relay number out of range, an invalid state, or a relay that is not configured raises an error.

## Constants

The two families are **not interchangeable**.

| Used by | Constants |
| :--- | :--- |
| `uart.open()` | `SERIAL_8N1`, `SERIAL_8N2`, `SERIAL_8E1`, `SERIAL_8E2`, `SERIAL_8O1`, `SERIAL_8O2`, `SERIAL_7N1`, `SERIAL_7N2`, `SERIAL_7E1`, `SERIAL_7E2`, `SERIAL_7O1`, `SERIAL_7O2` |
| `modbus.open()` | `RTU_8N1`, `RTU_8N2`, `RTU_8E1`, `RTU_8E2`, `RTU_8O1`, `RTU_8O2`, `RTU_7N1`, `RTU_7N2`, `RTU_7E1`, `RTU_7E2`, `RTU_7O1`, `RTU_7O2` |

The name reads as data bits, parity (N, E, O), stop bits.

## Serial ports

- Port `n` is UART `n`. The number of ports depends on the chip (`SOC_UART_NUM - 1`).
- Pins come from the pin configuration: `SERIAL<n>_TX`, `SERIAL<n>_RX` and the optional `SERIAL<n>_DE` for RS485.
- With `SERIAL<n>_DE` set, the line is held in receive mode and switches to transmit only while sending.
- A port has one owner at a time. Opening a port in use returns `"Port used by another owner"` or `"Port already open"`.

## Examples

- [`examples/hardware/uart.ti`](../../../../examples/hardware/uart.ti)
- [`examples/hardware/modbus.ti`](../../../../examples/hardware/modbus.ti)
- [`examples/hardware/relay.ti`](../../../../examples/hardware/relay.ti)
