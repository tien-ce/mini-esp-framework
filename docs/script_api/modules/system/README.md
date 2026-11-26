# Module `system`

The device, time since boot, running scripts and firmware signals. This module has no handles.

[Back to the API index](../../README.md)

```c
import system;
```

| Group | Functions |
| :--- | :--- |
| [Device](#device) | `info`, `free_heap`, `reset_reason`, `restart` |
| [Time since boot](#time-since-boot) | `uptime_s`, `uptime_ms`, `elapsed_ms` |
| [Scripts](#scripts) | `run`, `stop`, `scripts` |
| [Signals](#signals) | `subscribe`, `unsubscribe`, `SIG_xxx` |

---

## Device

| Function | Returns | Description |
| :--- | :--- | :--- |
| `info()` | `dict` | `model`, `chip`, `fw_version`, `sdk_version` (string), `flash_size`, `total_heap`, `total_psram` (int, bytes) |
| `free_heap()` | `int` | Free internal heap in bytes |
| `reset_reason()` | `string` | Why the board last restarted, for example `"POWERON"`, `"PANIC"`, `"SOFTWARE"` |
| `restart()` | void | Restart the board. The script keeps running until the board goes down |

## Time since boot

| Function | Returns | Description |
| :--- | :--- | :--- |
| `uptime_s()` | `int` | Seconds since boot |
| `uptime_ms()` | `int` | Millisecond tick. Wraps after about 24.8 days and can be negative |
| `elapsed_ms(int since)` | `int` | Milliseconds from the tick `since` to now, correct across the wrap |

Never add to or subtract from `uptime_ms()`. Keep it as a start point and pass it to `elapsed_ms`:

```c
int start = system.uptime_ms();
// ...
if (system.elapsed_ms(start) >= 5000) {
    start = system.uptime_ms();
}
```

## Scripts

| Function | Returns | Description |
| :--- | :--- | :--- |
| `run(string path)` | `bool` | Start a `.ti` file as a new script. Its name is its path |
| `stop(string name)` | `bool` | Ask the script to stop. `false` when no script has that name |
| `scripts()` | `list string` | Names of the running scripts |

- `run` returns `false` when the file is not a `.ti` file, cannot be read, or a script with that name already runs.
- `true` only means the script was started: a compile error appears later in the log.
- Script names are limited to 31 characters and at most 10 scripts run at once.
- A script stops at its next safe point, not instantly.

## Signals

A script subscribes **one** handler, which is called for **every** firmware signal and picks the ones it needs.

```c
void on_signal(int sig) {
    if (sig == system.SIG_1SEC) {
        print("One second");
    }
}

system.subscribe(on_signal);
while (true) {
    delay(1000);                 // the handler runs while the script waits
}
```

| Function | Description |
| :--- | :--- |
| `subscribe(func handler)` | Subscribe this script. Calling it again replaces the handler |
| `unsubscribe()` | Remove this script's subscription |

The subscription ends by itself when the script ends. A handler with the wrong parameters is reported in the log and removed.

| Signal | Mode | Fires | Should the handler act on it? |
| :--- | :---: | :--- | :--- |
| `SIG_INIT` | | Never reaches scripts | No |
| `SIG_10MS` | async | Every 10 ms | **Avoid**, ignore it and return at once |
| `SIG_100MS` | async | Every 100 ms | Possible, keep it very short |
| `SIG_1SEC` | async | Every second | Yes, for periodic work |
| `SIG_WIFI_CONNECTED` | async | Wi-Fi link came up | Yes |
| `SIG_WIFI_DISCONNECTED` | async | Wi-Fi reconnect started | Yes |
| `SIG_WEB_POLL` | **sync** | Each `GET /api/telemetry` | Only to copy data |
| `SIG_MQTT_PUBLISH` | **sync** | Before each MQTT telemetry publish | Only to copy data |

**Async signals** are queued and run at the script's next safe point. The queue holds 32 events per script, and new ones are dropped when it is full.

**Sync signals** make the web or MQTT task wait for the handler (up to 200 ms to reach it). Inside these branches:

- Do only quick RAM work: read variables, call `mqtt_update(...)` or `web_ui_update(...)`.
- Do **not** call `delay()`, HTTP, file access, Modbus or long loops.
- Keep the call depth shallow: the handler runs on the stack of the task that raised the signal.

A slow handler delays the page or the publish, or the data is sent without your values.

## Examples

- [`examples/system/device_info.ti`](../../../../examples/system/device_info.ti)
- [`examples/system/signals.ti`](../../../../examples/system/signals.ti)
- [`examples/system/elapsed_time.ti`](../../../../examples/system/elapsed_time.ti)
