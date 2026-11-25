# Network Built-ins

HTTP client, web UI updates and MQTT telemetry.

Sources: `src/scripting/builtins/builtin_http.cpp`, `builtin_web.cpp`, `builtin_mqtt.cpp`.

[Back to the index](../builtin_function_doc.md)

---

## HTTP

### `http_get(url)` / `http_post(url, data, [content_type])`
- **Module**: `builtin_http.cpp`, `builtin_web.cpp`
- **Prototype**:
  - `dict http_get(string url)`
  - `dict http_post(string url, string data, [string content_type])`
- **Return Value**: a `dict` with `code` (integer HTTP status) and `payload` (string response body).
- **Failure Behavior (fast-fail)**:
  - If the core network state is **not connected** (`CoreState_GetNetwork() != NET_STATE_WIFI_STA`), it returns `{"code": -1, "payload": "Network Disconnected"}` immediately, without blocking or touching LwIP.
  - Connection / network failure (DNS error, etc.): `code` is a negative integer (`-1` to `-11`) and `payload` holds the error text.

---

## Web UI

### `web_ui_update(element_id, value)`
- **Module**: `builtin_web.cpp`
- **Prototype**: `bool web_ui_update(string element_id, <auto> value)`
- **Parameters**:
  - `element_id`: target DOM element ID.
  - `value`: data to send (`int`, `float`, `bool`, `string`).
- **Return Value**: `true` on success.
- **Failure Behavior**: halts with `ti_fatal()` if arguments are invalid or the type is unsupported.

---

## MQTT

### `mqtt_update(key, value)`
- **Module**: `builtin_mqtt.cpp`
- **Prototype**: `bool mqtt_update(string key, <auto> value)`
- **Parameters**:
  - `key` (`string`): telemetry key.
  - `value`: `int`, `float`, `bool` or `string`.
- **Return Value**: `true` if the pair was stored for the next MQTT payload, `false` otherwise.
- **Failure Behavior**: halts with `ti_fatal()` if the key is not a string or the value type is unsupported.
