# TI Script API

Everything a `.ti` script can call on this firmware. For the language itself (types, loops, functions, `import`), see the guide of the interpreter in `lib/TIinterpreter/docs/guide/`.

## How the API is organized

| Level | What it is | How a script gets it | Example |
| :--- | :--- | :--- | :--- |
| **Global function** | Always available | Nothing to do | `print("hello");` |
| **Module** | A group of functions and constants for one domain | `import <module>;` | `system.uptime_s()` |
| **Handle** | An object that holds one resource (a port, a file, a queue) | Only from a function of its module | `handle uart = hw.UART();` |

```c
import hardware as hw;           // 1. import the module

handle uart = hw.UART();         // 2. the module creates the handle
print(uart.open(1, 9600, hw.SERIAL_8N1));
uart.write("Hello\n");           // 3. use the methods of the handle
uart.close();
```

Reading a name tells you what it is:

| Form | Meaning | Example |
| :--- | :--- | :--- |
| `module.UPPERCASE()` | Creates a handle | `storage.NVS()` |
| `module.lowercase()` | Plain function | `hardware.relay_get(1)` |
| `module.UPPERCASE` | Constant | `system.SIG_1SEC` |
| `value.lowercase()` | Method of a handle | `queue.send(1)` |

## Modules and handles

| Module | Domain | Creates handles | Functions and constants |
| :--- | :--- | :--- | :--- |
| [`hardware`](modules/hardware/README.md) | Ports, inputs and outputs of the board | [`UART()`](modules/hardware/uart.md), [`MODBUS()`](modules/hardware/modbus.md), [`PE_SENSOR()`](modules/hardware/photoelectric.md) | relays, frame constants |
| [`storage`](modules/storage/README.md) | Data that survives a restart | [`NVS()`](modules/storage/nvs.md), [`FILE()`](modules/storage/file.md) | none |
| [`ipc`](modules/ipc/README.md) | Links between scripts | [`QUEUE()`](modules/ipc/queue.md) | none |
| [`system`](modules/system/README.md) | Device, time, scripts, firmware signals | none | device info, uptime, run / stop, signals |
| [`network`](modules/network/README.md) | Requests to other machines | [`HTTP()`](modules/network/http.md) | none |

Functions that need no import (`print`, `delay`, `http_post`, `mqtt_update`, ...) are in [Global functions](global_functions.md).

Runnable scripts for each module are in [`examples/`](../../examples).

## Rules shared by every handle

- **Created by its module only.** There is no other way to get one.
- **Open, use, close.** Port and storage handles are created closed. Calling a method before `open()` raises an error. `close()` lets the same handle be opened again.
- **Released automatically.** When the last value pointing to a handle is gone (out of scope, script ended or stopped), the resource is closed.
- **Copies share the object.** After `handle b = a;`, both names use the same port.
- **A port has one owner.** A UART port used by a `uart` handle cannot be opened by a `modbus` handle at the same time. Queues are the exception: they are meant to be shared by name.
- **Waiting is not blocking.** `http.get`, `http.post` and `modbus.request` wait for their answer, but callbacks (signal handlers, `on_receive`, `on_done`) still run meanwhile. A callback must not use the handle the main code is waiting on: that raises an error. See [`http`](modules/network/http.md#waiting-is-not-blocking) and [`modbus`](modules/hardware/modbus.md#waiting-is-not-blocking).

## How errors are reported

| Kind of failure | Examples | What the script sees |
| :--- | :--- | :--- |
| **Mistake in the script** | Port out of range, wrong type, method on a closed handle | An error is **raised**: the script stops and the log shows the kind, message and line |
| **State or hardware** | Port busy, device did not answer, queue full | A **return value** the script can check: `"OK"` or an error text, `false`, `null`, or a `status` field |

Each page says which form its functions use.

## Adding to the API

**A new handle in an existing module**

1. Add `include/scripting/handle/<module>/<name>_handle.h` and `src/scripting/handle/<module>/<name>_handle.cpp`, defining `g_<name>_handle_type`.
2. Add its `UPPERCASE()` function to `src/scripting/module/<module>_module.cpp`.
3. Copy [`templates/handle.md`](templates/handle.md) to `modules/<module>/<name>.md`.
4. Link it from the module page and from the table above.

**A new module**

1. Add `src/scripting/module/<name>_module.cpp` with `<name>_module_init()`, which calls `ti_register_module()`.
2. Declare the init function in `include/scripting/module/module_manager.h` and call it in `modules_init()`.
3. Copy [`templates/module.md`](templates/module.md) to `modules/<name>/README.md`.
4. Add a row to the table above and an example under `examples/<name>/`.

**A new global function:** prefer a module. Add a global function only for something every script needs without an import, and document it in [Global functions](global_functions.md).
