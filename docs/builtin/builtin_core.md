# Core Built-ins

Console output, system helpers, script control, persistent storage (NVS) and JSON helpers.

Sources: `src/scripting/builtins/builtin_console.cpp`, `builtin_system.cpp`, `builtin_script.cpp`, `builtin_nvs.cpp`, `builtin_json.cpp`.

[Back to the index](../builtin_function_doc.md)

---

## Console

### `print(...)` / `print_info(...)`
- **Module**: `builtin_console.cpp`
- **Prototype**: `null print(val1, val2, ...)`
- **Parameters**: 0 to N arguments (`int`, `float`, `string`, `bool`, `null`).
- **Return Value**: Always `null`.
- **Failure Behavior**:
  - Null or uninitialized arguments are printed as `"null"`.
  - An empty argument list prints a blank line.
  - Never causes a system crash.

### `print_error(...)`, `print_warning(...)`, `print_debug(...)`
- Same arguments and return value as `print`, logged at the matching level.

---

## System

### `delay(ms)`
- **Module**: `builtin_system.cpp`
- **Prototype**: `null delay(int milliseconds)`
- **Parameters**:
  - `milliseconds` (`int` or `float`): time to pause in ms.
- **Return Value**: `null`
- **Behavior**: waits with `ti_wait`, which releases the runtime lock while sleeping, so other scripts and callbacks keep running.
- **Failure Behavior**:
  - Missing, null or wrong-type argument: logs `[ERROR]` and halts the interpreter task via `ti_fatal()`.
  - Values `<= 0` return immediately.

### `is_none(var)`
- **Module**: `builtin_system.cpp`
- **Prototype**: `bool is_none(var)`
- **Parameters**: `var`: any variable or expression result.
- **Return Value**: `true` if `var` is `null` (e.g. from a failed `file_read` or `get_json`), otherwise `false`.

### `get_device_ip()`
- **Module**: `builtin_system.cpp` (data from the Wi-Fi service, `get_wifi_local_ip()`)
- **Prototype**: `string get_device_ip()`
- **Return Value**: current IP address as text, or `""` when Wi-Fi is not connected or has no address yet. `"0.0.0.0"` is never returned.

### `get_device_info()`
- **Module**: `builtin_system.cpp`
- **Prototype**: `dict get_device_info()`
- **Return Value**: a new dict (a snapshot at call time). The builtin does not call the network stack itself: values come from the Wi-Fi service (`wifi_task`), the info provider (`core/info.h`) and the core state, so a new field is added in one place.

| Key | Type | Source | Meaning |
| :--- | :--- | :--- | :--- |
| `ip` | `string` | Wi-Fi service | Current IP, `""` when not connected |
| `hostname` | `string` | Wi-Fi config | Configured Client ID / hostname |
| `ssid` | `string` | Wi-Fi config | Configured SSID |
| `wifi_connected` | `bool` | Wi-Fi service | Link is up |
| `rssi` | `int` | Wi-Fi service | Signal in dBm, `0` when not connected |
| `mqtt_connected` | `bool` | Core state | MQTT session is established |
| `mac` | `string` | Info provider | MAC address |
| `model` | `string` | Info provider | Chip model |
| `fw_version` | `string` | Info provider | Firmware version |
| `uptime` | `int` | Info provider | Seconds since boot |
| `free_heap` | `int` | Info provider | Free heap in bytes |

```c
dict info = get_device_info();
print(info["ip"], info["rssi"]);
```
- **Failure Behavior**: if memory is exhausted while creating the dict, `TI_ERR_INVALID_ARG` is raised; individual fields that cannot be created are skipped.

---

## Script control

### `run_script(name, code)` / `script_run(name, code)`
- **Module**: `builtin_script.cpp`
- **Prototype**: `bool run_script(string name, string code)`
- **Parameters**:
  - `name`: identifier for the script.
  - `code`: the TI script source code to execute.
- **Return Value**: `true` on successful execution.
- **Failure Behavior**: halts with `ti_fatal()` if arguments are missing or not strings.

---

## NVS (Non-Volatile Storage)

### `nvs_write(key, value)`
- **Module**: `builtin_nvs.cpp`
- **Prototype**: `bool nvs_write(string key, <auto> value)`
- **Parameters**:
  - `key` (`string`): the NVS preference key.
  - `value`: data to write (`int`, `float`, `bool`, `string`).
- **Return Value**: `true` on success, `false` on failure.
- **Failure Behavior**: halts with `ti_fatal()` if an unsupported data type is provided.

### `nvs_read(key, default_value)`
- **Module**: `builtin_nvs.cpp`
- **Prototype**: `<auto> nvs_read(string key, <auto> default_value)`
- **Parameters**:
  - `key` (`string`): the NVS preference key.
  - `default_value`: fallback value, also used to infer the expected return type.
- **Return Value**: the stored value if found, otherwise `default_value`.

---

## JSON helpers

### `get_json(json_str, key)`
- **Module**: `builtin_json.cpp`
- **Prototype**: `<auto> get_json(string json_str, string key)`
- **Parameters**:
  - `json_str` (`string`): JSON formatted string.
  - `key` (`string`): property name to extract.
- **Return Value**: auto-converted value (`int`, `float`, `string`, `bool`).
- **Failure Behavior**: invalid JSON or a missing `key` returns `null`.

### Typed getters: `get_json_as_string`, `get_json_as_int`, `get_json_as_float`, `get_json_as_bool`
- Extract a specific primitive type. Return `null` if JSON parsing fails or the key is not found.
