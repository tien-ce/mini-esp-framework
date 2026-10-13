# Built-in Function Reference (`builtin_function_doc.md`)

This document defines all native built-in functions available in the **Tien Interpreter** (`.ti` scripts) across all framework modules, including **expected return values**, **failure/error behaviors**, and **recovery conventions**.

---

## 1. Summary of Built-in Functions & Failure Behaviors

| Function Name | Module Owner | Return Type | Normal Result | Failure / Error Result |
| :--- | :--- | :--- | :--- | :--- |
| `print(...)` | `builtin_console.cpp` | `null` | Logs output | `null` (never crashes on null/empty args) |
| `delay(ms)` | `builtin_system.cpp` | `null` | Pauses task | `ti_fatal()` if ms is invalid type |
| `is_none(var)` | `builtin_system.cpp` | `bool` | `true` if `null`, `false` otherwise | `ti_fatal()` if argument missing |
| `run_script(name, code)` | `builtin_script.cpp` | `bool` | `true` on success | `ti_fatal()` if invalid args |
| `script_run(name, code)` | `builtin_script.cpp` | `bool` | Alias for `run_script` | `ti_fatal()` if invalid args |
| `nvs_write(key, val)` | `builtin_nvs.cpp` | `bool` | `true` | `false` if NVS write fails; `ti_fatal()` if type unsupported |
| `nvs_read(key, def)` | `builtin_nvs.cpp` | `auto` | Extracted value | Returns `def` if key missing / read error |
| `get_json(json, key)` | `builtin_json.cpp` | `auto` | Extracted value | `null` if JSON malformed or key missing |
| `get_json_as_string(...)` | `builtin_json.cpp` | `string` | Extracted string | `null` if JSON malformed or key missing |
| `get_json_as_int(...)` | `builtin_json.cpp` | `int` | Extracted integer | `null` if JSON malformed or key missing |
| `get_json_as_float(...)` | `builtin_json.cpp` | `float` | Extracted float | `null` if JSON malformed or key missing |
| `get_json_as_bool(...)` | `builtin_json.cpp` | `bool` | Extracted boolean | `null` if JSON malformed or key missing |
| `http_get(url)` | `builtin_web.cpp` | `dict` | `{"code": 200, "payload": "..."}` | `{"code": -1..-11, "payload": "<error_str>"}` |
| `http_post(url, data, [type])` | `builtin_web.cpp` | `dict` | `{"code": 200..204, "payload": "..."}` | `{"code": -1..-11, "payload": "<error_str>"}` |
| `web_ui_update(id, val)` | `builtin_web.cpp` | `bool` | `true` | `ti_fatal()` if id not string or unsupported value type |
| `file_read(path)` | `builtin_fs.cpp` | `string` / `null` | File contents string | `null` if file not found / unmounted |
| `file_write(path, data)` | `builtin_fs.cpp` | `bool` | `true` | `false` if disk full / unmounted / invalid path |
| `file_exists(path)` | `builtin_fs.cpp` | `bool` | `true` (exists) | `false` (does not exist or unmounted) |
| `file_remove(path)` | `builtin_fs.cpp` | `bool` | `true` (deleted) | `false` if file not found / cannot delete |
| `relay_get_state(relay_num)` | `xdrv_02_relay.cpp` | `int` | `1` (ON) or `0` (OFF) | `0` if relay inactive; `ti_fatal()` if relay_num invalid |
| `relay_set_state(relay_num, st)`| `xdrv_02_relay.cpp` | `bool` | `true` | `false` if relay inactive; `ti_fatal()` if invalid arg |
| `autonics_read(addr)` | `xadp_01_autonics_tk.cpp` | `dict` | `{"success": true, "pv": ..., "sv": ..., "unit": ...}` | `{"success": false, "error": "..."}` |

---

## 2. Core System & Script Functions

### `print(...)`
- **Module**: `src/builtins/builtin_console.cpp`
- **Prototype**: `null print(val1, val2, ...)`
- **Parameters**: 0 to N arguments (`int`, `float`, `string`, `bool`, `null`).
- **Return Value**: Always returns `null`.
- **Failure Behavior**:
  - Null or uninitialized arguments are safely printed as `"null"`.
  - Empty argument list prints a blank line.
  - Never causes a system crash.

---

### `delay(ms)`
- **Module**: `src/builtins/builtin_system.cpp`
- **Prototype**: `null delay(int milliseconds)`
- **Parameters**:
  - `milliseconds` (`int` or `float`): Time to pause in ms.
- **Return Value**: `null`
- **Failure Behavior**:
  - If argument is missing, null, or wrong type: logs `[ERROR]` and safely halts interpreter task via `ti_fatal()`.
  - Values `<= 0` return immediately without delaying.

---

### `is_none(var)`
- **Module**: `src/builtins/builtin_system.cpp`
- **Prototype**: `bool is_none(var)`
- **Parameters**:
  - `var`: Any variable or expression result.
- **Return Value**:
  - Returns `true` if `var` is `null` (e.g. from failed `file_read` or `get_json`).
  - Returns `false` if `var` contains a valid value.

---

### `run_script(name, code)` / `script_run(name, code)`
- **Module**: `src/builtins/builtin_script.cpp`
- **Prototype**: `bool run_script(string name, string code)`
- **Parameters**:
  - `name`: Identifier for the script.
  - `code`: The TI script source code to execute.
- **Return Value**: `true` on successful execution.
- **Failure Behavior**:
  - Halts with `ti_fatal()` if arguments are missing or not strings.

---

## 3. NVS (Non-Volatile Storage) Functions

### `nvs_write(key, value)`
- **Module**: `src/builtins/builtin_nvs.cpp`
- **Prototype**: `bool nvs_write(string key, <auto> value)`
- **Parameters**:
  - `key` (`string`): The NVS preference key.
  - `value`: Data to write (`int`, `float`, `bool`, `string`).
- **Return Value**: `true` on success, `false` on failure.
- **Failure Behavior**:
  - Halts with `ti_fatal()` if unsupported data type is provided.

### `nvs_read(key, default_value)`
- **Module**: `src/builtins/builtin_nvs.cpp`
- **Prototype**: `<auto> nvs_read(string key, <auto> default_value)`
- **Parameters**:
  - `key` (`string`): The NVS preference key.
  - `default_value`: Fallback value, also used to automatically infer the expected return type.
- **Return Value**: Returns the stored value if found; otherwise, returns the `default_value`.

---

## 4. JSON Helper Functions

### `get_json(json_str, key)`
- **Module**: `src/builtins/builtin_json.cpp`
- **Prototype**: `<auto> get_json(string json_str, string key)`
- **Parameters**:
  - `json_str` (`string`): JSON formatted string.
  - `key` (`string`): Property name to extract.
- **Return Value**: Auto-converted value (`int`, `float`, `string`, `bool`).
- **Failure Behavior**:
  - If `json_str` is invalid/corrupted JSON or `key` doesn't exist: returns `null`.

### Typed JSON Getters (`get_json_as_string`, `get_json_as_int`, `get_json_as_float`, `get_json_as_bool`)
- Extract specific primitive types. Return `null` if JSON parsing fails or the key is not found.

---

## 5. Web & HTTP Functions

### `http_get(url)` / `http_post(url, data, [content_type])`
- **Module**: `src/builtins/builtin_web.cpp`
- **Prototype**:
  - `dict http_get(string url)`
  - `dict http_post(string url, string data, [string content_type])`
- **Return Value**: Returns a `dict` with two properties: `code` (integer HTTP status) and `payload` (string response body).
- **Failure Behavior (Fast-Fail)**:
  - If the core system network state is **not connected** (`CoreState_GetNetwork() != NET_STATE_WIFI_STA`), it immediately returns `{"code": -1, "payload": "Network Disconnected"}` without blocking or crashing LwIP.
  - **Connection / Network Failure** (DNS error, etc): `code` is a negative integer (`-1` to `-11`) and `payload` contains the error string.

### `web_ui_update(element_id, value)`
- **Module**: `src/builtins/builtin_web.cpp`
- **Prototype**: `bool web_ui_update(string element_id, <auto> value)`
- **Parameters**:
  - `element_id`: Target DOM element ID.
  - `value`: Data to send (`int`, `float`, `bool`, `string`).
- **Return Value**: `true` on success.
- **Failure Behavior**:
  - Halts with `ti_fatal()` if arguments are invalid or unsupported type.

---

## 6. File System Functions (LittleFS)

### `file_read(path)`
- **Module**: `src/builtins/builtin_fs.cpp`
- **Prototype**: `string file_read(string path)`
- **Failure Behavior**:
  - Returns `null` if file doesn't exist. Use `is_none(content)` to verify.

### `file_write(path, data)`
- **Module**: `src/builtins/builtin_fs.cpp`
- **Prototype**: `bool file_write(string path, string data)`
- **Failure Behavior**:
  - Returns `false` if LittleFS is unmounted or Flash disk is full.

### `file_exists(path)` & `file_remove(path)`
- Returns boolean indicating success.

---

## 7. Hardware Driver Functions (Relay)

### `relay_get_state(relay_num)`
- **Module**: `src/driver/xdrv_02_relay.cpp`
- **Prototype**: `int relay_get_state(int relay_num)`
- **Failure Behavior**:
  - Returns `0` if relay inactive. `ti_fatal()` if out of range (e.g. < 1 or > 3).

### `relay_set_state(relay_num, state)`
- **Module**: `src/driver/xdrv_02_relay.cpp`
- **Prototype**: `bool relay_set_state(int relay_num, <int/bool/string> state)`
- **Failure Behavior**:
  - Returns `false` if inactive. `ti_fatal()` if invalid arg.

---

## 8. Sensor Functions (Autonics TK Temperature Controller)

### `autonics_read(slave_address)`
- **Module**: `src/adapter/xadp_01_autonics_tk.cpp`
- **Prototype**: `dict autonics_read(int slave_address)`
- **Return Value**: Returns a `dict` with `success` (boolean). If true, includes `pv`, `sv` (floats), and `unit` (string). If false, includes `error` (string).
- **Failure Behavior**:
  - Returns `{"success": false, "error": "Invalid response"}` or specific modbus error if read fails.
