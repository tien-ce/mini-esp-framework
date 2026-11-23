# Global functions

Available in every script without `import`.

[Back to the API index](README.md)

| Group | Functions |
| :--- | :--- |
| [Console](#console) | `print`, `print_info`, `print_warning`, `print_error`, `print_debug` |
| [Timing](#timing) | `delay` |
| [Files](#files) | `file_read`, `file_write`, `file_exists`, `file_remove` |
| [JSON](#json) | `get_json`, `get_json_as_string`, `get_json_as_int`, `get_json_as_float`, `get_json_as_bool` |
| [HTTP](#http) | `http_get`, `http_post` |
| [MQTT](#mqtt) | `mqtt_update`, `mqtt_set_attribute`, `mqtt_alarm` |
| [Web UI](#web-ui) | `web_ui_update` |
| [Older functions](#older-functions) | `is_none`, `get_device_ip`, `get_device_info`, `run_script`, `script_run` |

---

## Console

```c
print(value, ...);
print_info(value, ...);      // same as print
print_warning(value, ...);
print_error(value, ...);
print_debug(value, ...);
```

- Joins the arguments without a separator and logs them at the matching level.
- Accepts `string`, `int`, `float` (2 decimals), `bool` and `null`.
- A `list`, `dict`, `bytes` or handle prints a placeholder (`<print: unprintable type N>`). Use `to_string()` for a list or dict.

## Timing

### `void delay(int ms)`

Pauses the script for `ms` milliseconds. A value `<= 0` returns at once. Events, signal handlers and other scripts keep running during the wait.

## Files

Simple one-call access to LittleFS. To keep a file open and append to it, use the [`file` handle](modules/storage/file.md).

| Function | Returns | On failure |
| :--- | :--- | :--- |
| `file_read(string path)` | `string`, the whole file | `null` (not found, not mounted) |
| `file_write(string path, string data)` | `bool`, replaces the file | `false` |
| `file_exists(string path)` | `bool` | `false` |
| `file_remove(string path)` | `bool` | `false` |

## JSON

Each call parses the JSON text and reads one top-level key.

| Function | Returns |
| :--- | :--- |
| `get_json(string json, string key)` | The value with its JSON type. An object or array comes back as JSON text |
| `get_json_as_string(json, key)` | `string` |
| `get_json_as_int(json, key)` | `int` |
| `get_json_as_float(json, key)` | `float` |
| `get_json_as_bool(json, key)` | `bool` |

All return `null` when the JSON is invalid, the key is missing, or its value is `null`.

## HTTP

```c
dict http_get(string url);
dict http_post(string url, string data);                // Content-Type: application/json
dict http_post(string url, string data, string type);
```

Both return `{"code": int, "payload": string}`.

- `code` is the HTTP status, or a negative number on a connection failure with the reason in `payload`.
- Without a Wi-Fi connection they return `{"code": -1, "payload": "Network Disconnected"}` at once.
- The script **waits for the answer, up to 5 s**. Do slow requests in a separate worker script ([`ipc`](modules/ipc/README.md)) and never inside a signal handler.

## MQTT

Each function takes either one pair or a dict of pairs. Values are `int`, `float`, `bool` or `string`. In the dict form, other value types are skipped.

### `bool mqtt_update(string key, value)` / `bool mqtt_update(dict values)`

Stores telemetry for the next MQTT payload. Returns `true` when every pair was stored.

### `bool mqtt_set_attribute(string key, value)` / `bool mqtt_set_attribute(dict values)`

For values that rarely change (serial number, setpoint, ...). The value is stored and is not sent with every telemetry payload. When it differs from the stored value, the MQTT task publishes **all** attributes once, retained, on `IOT-MONITOR/<DEVICE_ID>/ATTR` (checked about once per second, and again after every reconnect). Setting the same value again sends nothing.

### `bool mqtt_alarm(string code, string msg, string source, string level, bool active)`

Queues an alarm for the central monitor. Returns `true` when queued.

## Web UI

### `bool web_ui_update(string element_id, value)` / `bool web_ui_update(dict values)`

Updates elements of the web template page. Values are `int`, `float`, `bool` or `string`. In the dict form, each key is an element id and other value types are skipped.

## Older functions

They still work, but new scripts should use the replacement.

| Function | Use instead | Notes |
| :--- | :--- | :--- |
| `is_none(value)` | `value == null` | |
| `get_device_info()` | [`system.info()`](modules/system/README.md) | Also has network fields: `ip`, `hostname`, `ssid`, `wifi_connected`, `rssi`, `mqtt_connected`, `mac`, plus `model`, `fw_version`, `uptime`, `free_heap` |
| `get_device_ip()` | no replacement yet | Current IP, `""` when not connected |
| `run_script(name, code)`, `script_run(name, code)` | [`system.run(path)`](modules/system/README.md) | Runs source code from a string. Always returns `true`, even when the script did not start |
