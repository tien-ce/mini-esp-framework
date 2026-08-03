# Core Engine Architecture Specification
                      ┌───────────────────────────┐
                      │         main.cpp          │
                      └─────────────┬─────────────┘
                                    │ Calls
                                    ▼
                      ┌───────────────────────────┐
                      │    CoreEngine_Start()     │
                      └─────────────┬─────────────┘
                                    │ Boots Infrastructure
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                                 CORE ENGINE                                 │
│                                                                             │
│  ┌──────────────┐          CoreEventBus_Post()          ┌────────────────┐ │
│  │  wifi_task   ├──────────────────────────────────────►│                │ │
│  └──────────────┘                                       │                │ │
│  ┌──────────────┐       CoreRegistry_SetKeyValue()      │  CORE EVENT    │ │
│  │ config_mngr  ├──────────────────────────────────────►│  BUS & QUEUES  │ │
│  └──────────────┘                                       │  (FreeRTOS)    │ │
│  ┌──────────────┐          CoreLog_Publish()            │                │ │
│  │   log_task   ├──────────────────────────────────────►│                │ │
│  └──────────────┘                                       └───────┬────────┘ │
│                                                                 │          │
│                                    ┌────────────────────────────┼──────────┘
│                                    │ CoreEventBus_Receive()     │
│                                    ▼                            ▼
│                            ┌──────────────┐             ┌──────────────┐
│                            │  web_server  │             │  mqtt_task   │
│                            └──────────────┘             └──────────────┘
└────────────────────────────────────▲────────────────────────────▲───────────┘
│                            │
│ Updates State / Events     │
│ via Thread-Safe APIs       │
┌────────────────────────────────────┴────────────────────────────┴───────────┐
│                            APPLICATION LAYER                                │
│                                                                             │
│    ┌──────────────────┐    ┌──────────────────┐    ┌──────────────────┐     │
│    │   vTaskNPK       │    │   vTaskDHT22     │    │   vTaskSensorN   │     │
│    └──────────────────┘    └──────────────────┘    └──────────────────┘     │
└─────────────────────────────────────────────────────────────────────────────┘


---

## Communication Methods Specification

| Communication Method Name | Mechanism Used | Source Module | Destination Module | Purpose & Function |
| :--- | :--- | :--- | :--- | :--- |
| **`CoreEngine_Start()`** | Direct Function Call | `main.cpp` | `core_engine` | Initial bootstrapping entry point; loads NVS, initializes queues, and starts core tasks. |
| **`CoreEventBus_Post()`** | FreeRTOS Queue Push | `wifi_task`, System Tasks | Core Event Router | Emits lifecycle state change events (e.g., `EVENT_WIFI_CONNECTED`, `EVENT_WIFI_DISCONNECTED`). |
| **`CoreEventBus_Receive()`** | FreeRTOS Blocking Queue Pop | Core Event Router | `web_server`, `mqtt_task` | Blocks network consumer tasks until specific system events are fired to trigger service startup/shutdown. |
| **`CoreRegistry_SetKeyValue()`** | Thread-Safe Mutex Lock | Sensor Drivers (`vTaskNPK`) | `config_manager` / Shared RAM | Thread-safe writer method allowing application tasks to publish sensor data to central memory without calling network APIs. |
| **`CoreRegistry_GetKeyValue()`** | Thread-Safe Mutex Read | `web_server`, `mqtt_task` | Consumer / Exporter | Thread-safe reader method allowing WebServer and MQTT engines to extract sensor metrics dynamically. |
| **`CoreLog_Publish()`** | FreeRTOS Inter-Task Queue | Any Core/App Task | `log_task` | Asynchronous, thread-safe logging interface passing log strings and command routing packets to the central logger. |

---

## Architectural Principles

1. **Self-Contained Lifecycle Management:**
   * `main.cpp` initiates `CoreEngine_Start()` once and relinquishes execution control.
   * `web_server` and `mqtt_task` stay blocked on `CoreEventBus_Receive()` until `wifi_task` broadcasts `EVENT_WIFI_CONNECTED` via `CoreEventBus_Post()`.

2. **Decoupled Application Layer:**
   * Sensor tasks (e.g., `vTaskNPK`) execute independently of network connection states.
   * Hardware drivers interface with the Core exclusively through thread-safe state access (`CoreRegistry_SetKeyValue()`) and logger channels (`CoreLog_Publish()`).
