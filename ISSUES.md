# Known Issues & Technical Debt

## 1. Web Server / Frontend Configuration Save Redirect
- **Location:** `include/html/config_module_html.h` & `src/core/web_server_task.cpp`
- **Issue:** When submitting the `/config-module` form (`POST /saveModule`), the browser navigates directly to `/saveModule` and displays a plain text `"OK"` response page instead of returning the user to the Main Menu UI.
- **Fix:** Update frontend JavaScript in `config_module_html.h` to intercept form submission via `fetch()`, display a "Saving & Restarting..." status button, and automatically redirect `window.location.href = '/'` after the restart delay.

## 2. Web Polling Telemetry Value Representation
- **Location:** `include/core/web_server_task.h` & `src/core/web_server_task.cpp` (`updateElementValue()`)
- **Issue:** Web polling telemetry uses `String` representation for all dynamic values rather than leveraging native JSON data types (integers, floats, booleans).
- **Fix:** Refactor `updateElementValue()` API / `telemetryDoc` to accept typed parameters (e.g., `float`, `int`, `bool`) so response JSON contains proper numerical/boolean types instead of quoted strings.

## 3. Double Conversion in Sensor Drivers (`xsns_01_autonics_tk.cpp`)
- **Location:** `src/sensor/xsns_01_autonics_tk.cpp`
- **Issue:** `FormatValueWithDecimal()` formats raw register integer values into string representation (`pv_str`, `sv_str`), but `SIG_1SEC` immediately calls `atof(sv_str)` to pass to `rule_on_event()`.
- **Fix:** Update `FormatValueWithDecimal` or sensor helper functions to export `float` directly (e.g., `raw_val / pow(10, dp)`) for rules/events, avoiding string formatting followed immediately by float parsing (`atof`).

## 4. Incorrect IP Address Substitution (SSID instead of IP)
- **Location:** `src/core/web_server_task.cpp` (line 121) & `src/core/mqtt_task.cpp` (line 178)
- **Issue:** `%IP_ADDR%` template rendering and `mqttDoc["ip"]` use `getWifiSSID()` instead of `WiFi.localIP().toString()`, outputting the WiFi SSID string instead of the IP address.
- **Fix:** Replace `getWifiSSID()` calls for IP address placeholders with `WiFi.localIP().toString()`.

## 5. Data Race / Concurrent Mutation of `mqttDoc`
- **Location:** `src/core/mqtt_task.cpp` (lines 187, 210)
- **Issue:** `mqtt_add_telemetry()` mutates `mqttDoc` without acquiring `mqttMutex`. When drivers handle `SIG_MQTT_PUBLISH` in `DispatcherTask` on Core 1, concurrent reads/writes on `mqttDoc` in `vMqttTask` cause data races and memory crashes.
- **Fix:** Protect `mqttDoc` mutations inside `mqtt_add_telemetry()` with `mqttMutex`.

## 6. Stack Size Overflow & Inline Blocking Restarts in Web Endpoints [RESOLVED ✓]
- **Location:** `src/core/web_server_task.cpp` (lines 310-341, 378, 405)
- **Status:** **RESOLVED ✓**
- **Resolution:** Stack sizes increased to 4096 bytes for `save_restart_task` and `save_wifi_task`, and 2048 bytes for `deferred_restart`. `/saveConfig` now delegates LittleFS saving and system restart to a non-blocking background FreeRTOS task.

## 7. Driver Unregistration on Boot (`SIG_INIT`) [INTENDED DESIGN ℹ]
- **Location:** `src/core/dispatcher.cpp` (line 104) & `src/driver/xdrv_01_example.cpp`
- **Status:** **INTENDED DESIGN ℹ**
- **Note:** Framework design intentionally unregisters drivers from `s_xdrv_table` / `s_xsns_table` if `SIG_INIT` returns `false` (indicating hardware is unconfigured or absent). Drivers must return `true` during `SIG_INIT` to remain registered.

## 8. Memory Inefficiencies & Synchronous WebSocket Logging
- **Location:** `src/core/log_task.cpp` (lines 224-245)
- **Issue:** High-frequency dynamic `String` pass-by-value and concatenation cause heap fragmentation. Calling `ws.textAll()` synchronously inside `logPrint()` blocks execution tasks and allocates WebSocket client buffers heavily.
- **Fix:** Pass strings by const reference `const String&`, cache static tokens, and decouple WebSocket log output via a queue or buffer.

## 9. Pin Availability Check (`is_pin_used()`)
- **Location:** `src/core/pin_config.cpp` (line 88)
- **Issue:** `is_pin_used()` checks `g_pin_names[gpio].length() > 0`, but does not exclude `"None"`. Unassigned pins configured as `"None"` cause `is_pin_used()` to return `true` and `is_use_name("None")` to match unassigned pins.
- **Fix:** Update `is_pin_used()` to verify `g_pin_names[gpio].length() > 0 && !g_pin_names[gpio].equalsIgnoreCase("None")`.

## 10. Missing Persistence for Log Severity Level (`LOG_LEVEL`)
- **Location:** `src/core/log_task.cpp` (lines 33, 114–137)
- **Issue:** `currentLogLevel` is stored only in a static RAM variable (`static LogLevel currentLogLevel = LOG_LEVEL_DEBUG;`). When `setLogLevel()` updates the severity level via command (`CMD_SET_LEVEL`), the setting is lost upon reboot and defaults back to `LOG_LEVEL_DEBUG`.
- **Fix:** Register a config file (`register_config_file("log", "log_config.txt")`) and save/load `currentLogLevel` to/from LittleFS on change and boot.

## 11. Repeated Key-Value Configuration Parsing Pattern
- **Location:** `src/core/config_manager.cpp`, `src/core/wifi_task.cpp`, `src/core/web_server_task.cpp`, `src/core/mqtt_task.cpp`, `src/core/pin_config.cpp`, `src/core/rule_engine.cpp`, `src/driver/e3f_r2c1_count.cpp`
- **Issue:** The line-by-line key=value parsing loop (`raw.indexOf('\n')`, `line.indexOf('=')`, trimming key and value) is copy-pasted across 6+ different files.
- **Fix:** Package this repeated pattern into a reusable helper function in `config_manager.h` (e.g. `parse_kv_config(raw_content, callback)` or `std::unordered_map<String, String> parse_kv_config(raw_content)`).

## 12. Repeated Web HTTP Authentication Boilerplate
- **Location:** `src/core/web_server_task.cpp` (lines 237–440)
- **Issue:** The HTTP Basic Auth check `if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) return request->requestAuthentication();` is duplicated 12+ times across web endpoints.
- **Fix:** Simplify using a `#define AUTH_CHECK(req) if (!(req)->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) return (req)->requestAuthentication()` macro or middleware wrapper function.

## 13. Duplicated GPIO Lookup Logic in Drivers
- **Location:** `src/driver/e3f_r2c1_count.cpp` (lines 170–176)
- **Issue:** `vSensorTask` manually iterates over `MAX_GPIO_PINS` with string comparisons to resolve the pin number for `"E3FR2C1_IN"`, ignoring the existing `get_gpio_by_name()` helper in `pin_config.h`.
- **Fix:** Replace manual GPIO search loop with `int8_t e3f_pin = get_gpio_by_name("E3FR2C1_IN");`.

## 14. Duplicated Deferred Task Creation for System Restarts
- **Location:** `src/core/web_server_task.cpp` (lines 334–341, 374–378, 396–405, 446–455)
- **Issue:** FreeRTOS lambda task creation for deferred restarts (`vTaskDelay()` followed by `postIncomingCommand(CMD_RESTART)`) is duplicated across 4 endpoints.
- **Fix:** Create a single utility function `schedule_system_restart(uint32_t delay_ms)` to eliminate duplicated task creation logic.

## 15. High Heap Allocation Overhead in HTML Template Rendering
- **Location:** `src/core/web_server_task.cpp` (`renderTemplate()`, lines 106–127)
- **Issue:** `renderTemplate()` performs 15+ sequential `String::replace()` calls per page request, allocating and reallocating heap memory continuously during page loads.
- **Fix:** Optimize template rendering by pre-building dynamic values into a key-value dictionary or performing single-pass token substitution to minimize heap allocation.
