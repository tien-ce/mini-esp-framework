# Coding & Documentation Style Guide

This document defines the coding conventions, linkage rules, and Doxygen documentation placement for the `mini-esp-framework` codebase.

---

## 1. Function Linkage & Encapsulation Rules

- **Public APIs**: Functions intended to be called across multiple translation units must be declared in the corresponding `.h` header file and defined in `.cpp`.
- **Internal Helper Functions**: Any function or global variable that is only used within a single `.cpp` file **MUST** be marked `static` (internal linkage).

---

## 2. Doxygen Documentation Rules

- **Header Files (`.h`)**:
  - Place `/** @brief ... */` Doxygen blocks in the `.h` file for all public APIs.
  - Do **NOT** duplicate Doxygen comment blocks in the `.cpp` file for public header functions.
- **Source Files (`.cpp`)**:
  - Place `/** @brief ... */` Doxygen blocks in the `.cpp` file directly above `static` function definitions.
- **Description Length**:
  - Use short, concise single-line `@brief` descriptions for functions with self-explanatory names or simple operations.

---

## 3. Naming Conventions

- **Global & Module Utility APIs**: `snake_case`
  - Examples: `register_config_file()`, `is_wifi_connected()`, `esp_info_get_model()`, `register_cmd()`
- **Core System State Matrix APIs**: `CoreState_<Verb><Target>`
  - Examples: `CoreState_GetMode()`, `CoreState_SetNetwork()`, `CoreState_GetWebServer()`
- **FreeRTOS Tasks**: `v<TaskName>`
  - Examples: `vWifiTask()`, `vLogTask()`, `vWebMonitorTask()`, `vSensorTask()`
- **Hardware Pin Constants**: `IN_<N>`, `OUT_<N>`, or `GPIO_<N>` (uppercase macro definitions)
- **Local Variables & Parameters**: `camelCase` or `snake_case` (keep consistent within the module)

---

## 4. Header File Structure

- Protect all headers with `#ifndef` / `#define` / `#endif` include guards.
- Include only required system headers and core dependencies.

---

## 5. Built-in Functions & Interpreter Safety Rules

All native built-in functions registered to the Tien interpreter (`native_fn_t` callbacks) **MUST** strictly adhere to the following safety and architectural rules:

### 5.1. Zero-Crash & Worst-Case Argument Validation (BẮT BUỘC)
- **Zero Panic/Crash Guarantee**: Under NO circumstance should a built-in function cause a system crash, panic, core dump, or Segmentation Fault (Null Pointer Dereference).
- **Pointer & Argument Validation**:
  - Always validate `argc`, `argv != NULL`, and every element `argv[i] != NULL` before reading `argv[i]->type` or accessing union payload fields.
  - For string arguments (`VAL_STRING`), verify that `argv[i]->string_val != NULL` before calling standard C library functions (`strlen`, `strcmp`, etc.) or ESP32 `String()` constructors.
- **Fail-Safe Error Reporting**:
  - If argument count or argument types are invalid, log immediately using:
    `ti_log("[ERROR] <func_name>: <detailed reason>\n");`
    and terminate interpreter execution safely using `ti_fatal()`.
- **Return Value Safety**:
  - Never return a bare C `NULL` from a built-in function. Always return a valid `value_t*` (use `val_new_null()` on handled error or non-returning operations).

### 5.2. Module Ownership & Direct Registration
- **Domain Subsystems**: Built-in functions that interact with a specific module subsystem (e.g. LittleFS operations in `file_system.cpp`, logging in `log_task.cpp`) MUST be defined as `static` functions directly inside that module's `.cpp` file.
- **Initialization**: Register built-in functions using `register_builtin_function()` directly within that module's `init()` or task startup routine without declaring separate registration functions in public `.h` header files.
- **General Utilities**: General built-ins that have no dedicated subsystem (such as JSON parsing, task delay, HTTP methods) reside in `ti_interpreter.cpp`.

### 5.3. Multi-Value Return Convention (HTTP / Compound Results)
- For built-in operations that need to return multiple holding values (such as HTTP status code and response payload body), encode the result into a structured JSON string (`{"code": <status_code>, "payload": "<response_string>"}`) and return via `val_new_string()`.
- Scripts consume individual fields cleanly using the built-in `get_json()` function.

---

## 6. Concurrency & Mutex Synchronization Rules

- **Write Operations (Setters, Updaters & State Mutations)**:
  - **BẮT BUỘC**: Luôn sử dụng FreeRTOS Mutex (`xSemaphoreTake` / `xSemaphoreGive`) khi ghi, cập nhật biến trạng thái tĩnh nội bộ, hoặc lưu cấu hình vào NVS để đảm bảo tính an toàn đa luồng (Thread-safety) giữa các Task.
- **Read Operations (Getters & State Inquiries)**:
  - **KHÔNG sử dụng Mutex Lock cho các phương thức đọc (Read / Getter)**: Các hàm getter đơn giản (ví dụ: `getWifiSSID()`, `getWifiIP()`, `getWifiGateway()`, v.v.) chỉ đọc trực tiếp giá trị biến để tối ưu hiệu năng, giảm độ trễ (latency), tránh tranh chấp lock (contention) và hiện tượng đảo ngược quyền ưu tiên (priority inversion) khi được gọi liên tục trong các vòng lặp telemetry, logging, hoặc template rendering.

