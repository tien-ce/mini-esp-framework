# Modbus Built-ins

Generic Modbus RTU master for TI scripts. Source: `src/scripting/builtins/builtin_modbus.cpp` (compiled only with `USE_MODBUS_MASTER`). All calls go through the `modbus_master` wrapper, so scripts share the bus and the transaction lock with the hardware adapters.

[Back to the index](../builtin_function_doc.md)

> **Under migration.** The serial layer was split into `serial_bus` (pins and ownership) and protocol layers. `modbus_master` and these built-ins still have to move to the new bus API (and to a per-port Modbus instance), so the signatures below can change. A port opened with `modbus_open` is owned by Modbus and cannot be used by [`serial_raw`](builtin_serial_raw.md) at the same time.

---

### `modbus_open(port, baud, frame)`
- **Prototype**: `dict modbus_open(int port, int baud, string frame)`
- **Parameters**: `port` (`1..N`), `baud` (> 0), `frame` (`"8N1"`, `"7E2"`, ... same names as `serial_raw_open`).
- **Return Value**: `{"success": true}` or `{"success": false, "error": "<text>"}`.

### `modbus_read(slave, function, address, count)`
- **Prototype**: `dict modbus_read(int slave, int function, int address, int count)`
- **Parameters**: `slave` 1..247, `function` 3 (holding registers) or 4 (input registers), `address` 0..65535, `count` 1..`MODBUS_MASTER_MAX_REGS`.
- **Return Value**: `{"success": true, "regs": [int, ...], "data": bytes}` (`data` = the same registers as big-endian bytes), or `{"success": false, "error": "<text>"}`.

### `modbus_write(slave, function, address, value)`
- **Prototype**: `dict modbus_write(int slave, int function, int address, int value)`
- **Parameters**: `function` 6 (write register, `value` 0..65535) or 5 (write coil, `value` 0 or 1).
- **Return Value**: `{"success": true}` or `{"success": false, "error": "<text>"}`.

### Bytes helpers (CLI interpreter only)
`modbus_read_request(slave, address, count)` and `crc16(bytes)` exist in the TI command-line build (`cli/builtin/builtin_bytes.c`), not in the firmware.
