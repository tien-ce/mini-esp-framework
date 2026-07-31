# Industrial Sensor Counter (ESP32-S3)

An industrial-grade sensor counter firmware built for **ESP32-S3** using **PlatformIO** and **FreeRTOS**. Features high-frequency edge-detection pulse counting, asynchronous HTTP API logging, non-volatile flash configuration storage (NVS), a real-time WebSocket serial monitor, thread-safe colorized logging, event-driven command processing, and an embedded Web Dashboard supporting Over-The-Air (OTA) firmware updates.

---

## Modular File Structure

```
QA_Sensor_Count/
├── include/
│   ├── Header_sensor.h      # Hardware GPIO pin definitions & serial settings
│   ├── index_html.h         # Embedded Web UI (HTML, CSS, JavaScript) stored in Flash
│   ├── config_manager.h     # Declarations for NVS Flash configurations
│   ├── log_task.h           # Declarations for Logger, ANSI/HTML Color Macros, & Command Router
│   ├── wifi_task.h          # Declarations for WiFi manager & reconnection task
│   ├── web_server.h         # AsyncWebServer, WebSocket & REST endpoint declarations
│   └── freertos_tasks.h     # FreeRTOS task function declarations & event queues
├── src/
│   ├── config_manager.cpp   # Implementation of NVS Flash storage & thread-safe state access
│   ├── log_task.cpp         # Event-driven Log & Command processing task implementation
│   ├── wifi_task.cpp        # WiFi setup, disconnect handler & vWifiTask implementation
│   ├── web_server.cpp       # Implementation of AsyncWebServer REST API routes & WebSockets
│   ├── freertos_tasks.cpp   # Implementation of FreeRTOS tasks (Sensor, Network, Monitor)
│   └── sensor_count_QA.cpp  # Lightweight main entry point (setup() & loop())
├── platformio.ini           # PlatformIO project configuration file
├── FUTURE_COMMAND_REGISTRY.md # Architecture specification for relational command database
└── README.md                # Project documentation
```

---

## FreeRTOS Multitasking Architecture

The firmware utilizes **FreeRTOS** multi-tasking pinned to **Core 1** to guarantee real-time sensor sampling without being blocked by network requests or web server traffic.

| Task Name | Priority | Core | Description |
| :--- | :---: | :---: | :--- |
| **`vSensorTask`** | **3 (High)** | Core 1 | Polls digital input pin `IN_1` (GPIO 47) every 10ms. Performs rising-edge detection (`LOW` -> `HIGH`), safely increments counter `cnt`, and pushes count events to `sensorQueue`. |
| **`vWifiTask`** | **2 (Medium)** | Core 1 | Dedicated WiFi monitoring task. Periodically checks connection status and performs auto-reconnection in the background. |
| **`vLogTask`** | **2 (Medium)** | Core 1 | Pure event-driven Log & Command Task. Sleeps indefinitely on `commandQueue` (0% CPU idle) until a command is posted from Serial (UART RX interrupt) or Web (WebSocket callback), then executes it. |
| **`vWebMonitorTask`** | **1 (Low)** | Core 1 | Periodically cleans up inactive WebSocket clients and performs background system maintenance. |

### Thread Safety & Self-Managed Mutexes
Each module manages its own internal mutexes locally:
- **`config_manager`**: Self-manages `countMutex` (guards sensor count `cnt`) and `configMutex` (guards WiFi/API settings).
- **`log_task`**: Self-manages `logMutex` (guards thread-safe output to Serial and WebSocket clients).
- **`sensorQueue`**: FreeRTOS Inter-Task Queue holding count events passed asynchronously from `vSensorTask` to network handler.
- **`commandQueue`**: FreeRTOS Queue for passing `CommandPacket` data from Serial RX interrupt and Web WS callbacks to `vLogTask`.

---

## Colorized Logging & Command Registry

The logging system supports dual-format colorized output:
- **Terminal (Serial)**: Uses ANSI escape sequences (`LOG_COLOR_RED`, `LOG_COLOR_YELLOW`, `LOG_COLOR_GREEN`, `LOG_COLOR_BLUE`).
- **Web Terminal**: Uses HTML `<span>` color wrappers (`WEB_COLOR_RED`, `WEB_COLOR_YELLOW`, `WEB_COLOR_GREEN`, `WEB_COLOR_CYAN`).

---

## Web Dashboard & REST API

The embedded web server runs asynchronously on port **8088** protected by **HTTP Basic Authentication**.

### Web Endpoints

| Endpoint | Method | Description |
| :--- | :---: | :--- |
| `/` | `GET` | Main Web Interface Dashboard (Serial Monitor, Settings, OTA Update). |
| `/ws` | `WS` | Real-time WebSocket terminal output for live logging & web commands. |
| `/stats` | `GET` | Returns JSON system telemetry (`uptime`, `freeHeap`, `count`, `rssi`, `clientID`). |
| `/getConfig` | `GET` | Returns current device configurations in JSON format. |
| `/saveConfig` | `POST` | Saves updated WiFi and device configurations to Flash and restarts ESP32. |
| `/cmd` | `GET` | Triggers system commands (e.g., `/cmd?msg=SET_LOG_LEVEL:DEBUG`). |
| `/resetConfig`| `GET` | Factory resets flash settings to embedded defaults and restarts. |
| `/doUpdate` | `POST` | Handles Over-The-Air (OTA) binary firmware update uploads. |



## Building and Flashing

Build and upload the firmware using PlatformIO CLI:

```bash
# Compile project
pio run

# Upload firmware to ESP32-S3
pio run -t upload

# Open Serial Monitor
pio device monitor -b 9600
```
