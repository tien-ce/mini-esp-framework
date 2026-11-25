# Built-in Function Reference (`builtin_function_doc.md`)

This is the index of the native built-in functions available in the **Tien Interpreter** (`.ti` scripts). Each group has its own file under `docs/builtin/` with the prototypes, **expected return values**, **failure/error behaviors** and **recovery conventions**.

---

## 1. Groups

| Group | Doc | Sources | Contents |
| :--- | :--- | :--- | :--- |
| Core | [`builtin_core.md`](builtin/builtin_core.md) | `builtin_console`, `builtin_system`, `builtin_script`, `builtin_nvs`, `builtin_json` | print, delay, device info, run_script, NVS, JSON |
| Dispatcher | [`builtin_dispatcher.md`](builtin/builtin_dispatcher.md) | `builtin_dispatcher` | subscribe a script to firmware signals |
| Network | [`builtin_network.md`](builtin/builtin_network.md) | `builtin_http`, `builtin_web`, `builtin_mqtt` | HTTP client, web UI, MQTT telemetry |
| File system | [`builtin_filesystem.md`](builtin/builtin_filesystem.md) | `builtin_fs` | LittleFS files |
| Serial raw | [`builtin_serial_raw.md`](builtin/builtin_serial_raw.md) | `builtin_serial_raw` | plain bytes over a UART port, receive handler |
| Modbus | [`builtin_modbus.md`](builtin/builtin_modbus.md) | `builtin_modbus` | generic Modbus RTU master (under migration) |
| Hardware driver | [`builtin_hardware_driver.md`](builtin/builtin_hardware_driver.md) | `src/hardware/driver/` | on-board relays |
| Hardware adapter | [`builtin_hardware_adapter.md`](builtin/builtin_hardware_adapter.md) | `src/hardware/adapter/` | Autonics TK temperature controller |

Scripting built-ins live in `src/scripting/builtins/`. Driver and adapter built-ins are registered by their own module.

---

## 2. Summary of Built-in Functions & Failure Behaviors

| Function Name | Group | Return Type | Normal Result | Failure / Error Result |
| :--- | :--- | :--- | :--- | :--- |
| `print(...)` | Core | `null` | Logs output | `null` (never crashes on null/empty args) |
| `delay(ms)` | Core | `null` | Pauses the script (`ti_wait`) | `ti_fatal()` if ms is invalid type |
| `is_none(var)` | Core | `bool` | `true` if `null`, `false` otherwise | `ti_fatal()` if argument missing |
| `get_device_ip()` | Core | `string` | Current IP, e.g. `"192.168.1.50"` | `""` when Wi-Fi is not connected / no address yet |
| `get_device_info()` | Core | `dict` | Network, identity and runtime fields | `ti_raise_error` only on out-of-memory |
| `run_script(name, code)` / `script_run(...)` | Core | `bool` | `true` on success | `ti_fatal()` if invalid args |
| `nvs_write(key, val)` | Core | `bool` | `true` | `false` if NVS write fails; `ti_fatal()` if type unsupported |
| `nvs_read(key, def)` | Core | `auto` | Extracted value | Returns `def` if key missing / read error |
| `get_json(json, key)` | Core | `auto` | Extracted value | `null` if JSON malformed or key missing |
| `get_json_as_string/int/float/bool(...)` | Core | typed | Extracted value | `null` if JSON malformed or key missing |
| `dispatcher_register(handler)` | Dispatcher | `null` | Handler subscribed to all signals | `TI_ERR_INVALID_ARG` if the name is empty / too long |
| `dispatcher_unregister()` | Dispatcher | `null` | Subscription removed | no effect when not subscribed |
| `http_get(url)` | Network | `dict` | `{"code": 200, "payload": "..."}` | `{"code": -1..-11, "payload": "<error_str>"}` |
| `http_post(url, data, [type])` | Network | `dict` | `{"code": 200..204, "payload": "..."}` | `{"code": -1..-11, "payload": "<error_str>"}` |
| `web_ui_update(id, val)` | Network | `bool` | `true` | `ti_fatal()` if id not string or unsupported value type |
| `mqtt_update(key, val)` | Network | `bool` | `true` | `false` if not stored; `ti_fatal()` on unsupported type |
| `file_read(path)` | File system | `string` / `null` | File contents | `null` if file not found / unmounted |
| `file_write(path, data)` | File system | `bool` | `true` | `false` if disk full / unmounted / invalid path |
| `file_exists(path)` | File system | `bool` | `true` (exists) | `false` (does not exist or unmounted) |
| `file_remove(path)` | File system | `bool` | `true` (deleted) | `false` if file not found / cannot delete |
| `serial_raw_open(port, baud, frame)` | Serial raw | `string` | `"OK"` | error text; raises on a script mistake |
| `serial_raw_close(port)` | Serial raw | `string` | `"OK"` | error text; raises on a bad port |
| `serial_raw_write(port, data)` | Serial raw | `string` | `"OK"` | error text; raises if `data` is not string/bytes |
| `serial_raw_available(port)` | Serial raw | `int` | Bytes waiting | `0` when nothing / not open; raises on a bad port |
| `serial_raw_read(port, max_len)` | Serial raw | `dict` | `{"status": "OK", "data": bytes}` | `{"status": "<error text>", "data": <empty>}` |
| `serial_raw_on_receive(port, handler)` | Serial raw | `string` | `"OK"` | error text; raises on a bad port / handler name |
| `modbus_open(port, baud, frame)` | Modbus | `dict` | `{"success": true}` | `{"success": false, "error": "..."}` |
| `modbus_read(slave, fn, addr, count)` | Modbus | `dict` | `{"success": true, "regs": [...], "data": bytes}` | `{"success": false, "error": "..."}` |
| `modbus_write(slave, fn, addr, value)` | Modbus | `dict` | `{"success": true}` | `{"success": false, "error": "..."}` |
| `relay_get_state(relay_num)` | Hardware driver | `int` | `1` (ON) or `0` (OFF) | `0` if relay inactive; `ti_fatal()` if relay_num invalid |
| `relay_set_state(relay_num, st)` | Hardware driver | `bool` | `true` | `false` if relay inactive; `ti_fatal()` if invalid arg |
| `autonics_read(addr)` | Hardware adapter | `dict` | `{"success": true, "pv": ..., "sv": ..., "unit": ...}` | `{"success": false, "error": "..."}` |

---

## 3. Error conventions

New built-ins (the `serial_raw` group is the reference) report two kinds of failure differently:

- **A mistake in the script** (argument out of range, wrong type, unknown name): the native function calls `ti_raise_error` and returns `NULL`. The script stops with the error kind, message and line, so nothing has to be checked after every call.
- **A state or hardware condition** (busy, not open, device did not answer): the result carries it: `"OK"` or an error text for functions that return a string, a `status` / `success` field for functions that return a dict.

Older built-ins (`print`, `nvs_*`, `relay_*`, ...) use `ti_fatal()` or `null`/`false` as described in their own group.

---

## 4. Adding a built-in

1. Write the function in the matching `src/scripting/builtins/builtin_<group>.cpp` (or in the driver/adapter that owns the hardware) and register it in the group's `*_init()` with `register_builtin_function(name, return_type, params, count, fn)`.
2. Put the name macro in `include/built_in.h` when other code needs it, and add the `*_init()` call to `builtin_manager.cpp` for a new group.
3. Document it in the group's file under `docs/builtin/` and add its row to the table in section 2.
