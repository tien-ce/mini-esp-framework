# Mini ESP Framework

A modular, event-driven FreeRTOS firmware framework for ESP32 microcontrollers. The framework decouples hardware peripheral management, protocol adapters, script execution, and networking services through a centralized Event Bus and thread-safe registry.

---

## Directory Structure

```text
├── include/                 # Header files
│   ├── core/                # Core engine, system tasks, and web headers
│   ├── drivers/             # Hardware driver headers
│   └── html/                # Web dashboard HTML templates
├── lib/                     # External libraries and submodules
│   ├── chashmap/            # C Hashmap library
│   ├── Modbus_arduinoIDE/   # Modbus RTU/ASCII library
│   └── TIinterpreter/       # TI scripting language interpreter
├── src/                     # Application source code
│   ├── adapter/             # Protocol and sensor adapters
│   ├── builtins/            # Script interpreter built-in bindings
│   ├── core/                # Core engine, tasks, and web routing
│   ├── driver/              # Peripheral drivers
│   └── main.cpp             # Application entry point
├── test/                    # Unit and integration tests
└── platformio.ini           # PlatformIO project configuration
```

### Component Overview

- **`src/`**: Primary application source code.
  - **`main.cpp`**: Application entry point initializing the system via `CoreEngine_Start()`.
  - **`src/adapter/`**: Hardware and protocol adapters (e.g., Modbus sensor adapters).
  - **`src/builtins/`**: Built-in function bindings for the Tien script interpreter (FS, HTTP, JSON, NVS, System, Web, Console).
  - **`src/core/`**: Core infrastructure including the engine lifecycle, event dispatcher, config manager, file system, logger, Wi-Fi, MQTT, and WebServer/WebSockets (`src/core/web/`).
  - **`src/driver/`**: Low-level peripheral drivers (relays, digital counters, etc.).
- **`lib/`**: External libraries and Git submodules.
  - **[TI Language Interpreter](https://github.com/tien-ce/Write-your-own-interpreter-language)**: Embedded lightweight scripting and interpreter language engine.
  - **[MODBUS Library](https://github.com/tien-ce/Modbus_arduinoIDE)**: Industrial Modbus RTU/ASCII communication protocol library.
  - **[C Hashmap](https://github.com/tien-ce/chashmap)**: Generic C hashmap library providing key-value mapping and data storage.
- **`include/`**: Public and internal header files (`core/`, `drivers/`, `html/`, configuration and shared definitions).
