# Command Reference (`cmd_doc.md`)

This document specifies all CLI and WebSocket commands supported by the `mini-esp-framework` command parser (`log_task.cpp`).

---

## 1. Command Syntax & Calling Conventions

Commands can be dispatched to the system via **Hardware UART Serial**, **USB CDC Terminal**, or **WebSocket REST/WS Stream**.

### Supported Input Formats
- **Colon-delimited (`NAME: ARG`)**: Recommended for explicit arguments.
  ```text
  RELAY1: ON
  CMD_SET_LEVEL: DEBUG
  tien: /test_relay.ti
  ```
- **Space-delimited (`NAME ARG`)**:
  ```text
  tien /test_relay.ti
  ```
- **Parameterless (`NAME`)**:
  ```text
  CMD_GET_LEVEL
  CMD_LIST_LEVEL
  ESP32_RESTART
  ```

---

## 2. System & Core Commands

### `CMD_SET_LEVEL`
Sets the system runtime logging severity threshold.
- **Prototype / Syntax**: `CMD_SET_LEVEL: <level>`
- **Arguments**:
  - `level` (`string` or `int`): `"DEBUG"` / `0`, `"INFO"` / `1`, `"WARNING"` / `2`, `"ERROR"` / `3`.
- **Example**:
  ```text
  CMD_SET_LEVEL: DEBUG
  ```

### `CMD_GET_LEVEL`
Queries and logs the current active logging level.
- **Prototype / Syntax**: `CMD_GET_LEVEL`
- **Arguments**: None.
- **Example**:
  ```text
  CMD_GET_LEVEL
  ```

### `CMD_LIST_LEVEL`
Lists all valid log severity levels.
- **Prototype / Syntax**: `CMD_LIST_LEVEL`
- **Arguments**: None.
- **Example**:
  ```text
  CMD_LIST_LEVEL
  ```

### `ESP32_RESTART`
Flushes buffers and performs a software restart of the ESP32.
- **Prototype / Syntax**: `ESP32_RESTART`
- **Arguments**: None.
- **Example**:
  ```text
  ESP32_RESTART
  ```

### `BACKLOG`
Executes multiple semicolon-separated commands sequentially in FIFO order.
- **Prototype / Syntax**: `BACKLOG <cmd1:arg1>; <cmd2:arg2>; <cmd3>`
- **Arguments**: Semicolon-delimited sequence of valid commands.
- **Example**:
  ```text
  BACKLOG SSID: MyWifi; PASSWORD: MyPassword; ESP32_RESTART
  BACKLOG RELAY1: ON; RELAY2: ON; CMD_SET_LEVEL: DEBUG
  ```

---

## 3. WiFi Configuration Commands

### `SSID` / `WIFI_SSID`
Queries or updates WiFi Station SSID in NVS.
- **Prototype / Syntax**: `SSID` or `SSID: <ssid_name>`
- **Example**:
  ```text
  SSID: Home_Network
  SSID
  ```

### `PASSWORD` / `WIFI_PASS`
Queries or updates WiFi Station password in NVS.
- **Prototype / Syntax**: `PASSWORD` or `PASSWORD: <secret_pass>`
- **Example**:
  ```text
  PASSWORD: SecretPassword123
  ```

### `CLIENT_ID` / `DEVICE_ID` / `HOSTNAME`
Queries or updates Device Client ID / WiFi Hostname in NVS.
- **Prototype / Syntax**: `CLIENT_ID` or `CLIENT_ID: <device_name>`
- **Example**:
  ```text
  CLIENT_ID: ESP32_BOX_01
  CLIENT_ID
  ```

### `IPADDRESS` / `STATIC_IP`
Queries or updates Static IP address in NVS.
- **Prototype / Syntax**: `IPADDRESS` or `IPADDRESS: <ip_address>`
- **Example**:
  ```text
  IPADDRESS: 192.168.1.100
  ```

### `GATEWAY` / `STATIC_GATEWAY`
Queries or updates Static Gateway IP in NVS.
- **Prototype / Syntax**: `GATEWAY` or `GATEWAY: <gateway_ip>`
- **Example**:
  ```text
  GATEWAY: 192.168.1.1
  ```

### `SUBNETMASK` / `STATIC_SUBNET`
Queries or updates Static Subnet Mask in NVS.
- **Prototype / Syntax**: `SUBNETMASK` or `SUBNETMASK: <subnet_mask>`
- **Example**:
  ```text
  SUBNETMASK: 255.255.255.0
  ```

### `DNSSERVER` / `STATIC_DNS1`
Queries or updates Static DNS Server IP in NVS.
- **Prototype / Syntax**: `DNSSERVER` or `DNSSERVER: <dns_ip>`
- **Example**:
  ```text
  DNSSERVER: 8.8.8.8
  ```

### `WIFI_STATUS`
Displays complete WiFi configuration and live connection status.
- **Prototype / Syntax**: `WIFI_STATUS`
- **Example**:
  ```text
  WIFI_STATUS
  ```

---

## 4. Interpreter Command

### `tien`
Executes a `.ti` script file stored on the LittleFS flash storage inside a background FreeRTOS task.
- **Prototype / Syntax**: `tien: <filepath>` or `tien <filepath>`
- **Arguments**:
  - `filepath` (`string`): Absolute or relative path to the `.ti` script on LittleFS.
- **Example**:
  ```text
  tien /test_relay.ti
  tien: /http_request.ti
  ```

---

## 5. Hardware Driver Commands

### `RELAY1` / `RELAY2` / `RELAY3`
Controls the specified hardware relay output channel.
- **Prototype / Syntax**: `RELAY<N>: <state>` (where `<N>` is `1`, `2`, or `3`)
- **Arguments**:
  - `state` (`string` or `int`): `"1"`, `"ON"` (turns relay on), `"0"`, `"OFF"` (turns relay off).
- **Examples**:
  ```text
  RELAY1: ON
  RELAY1: 0
  RELAY2: 1
  RELAY3: OFF
  ```
