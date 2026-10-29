# Hardware Adapter Built-ins

Built-ins registered by device adapters (`src/hardware/adapter/`). An adapter talks to one kind of external device through a protocol layer (`modbus_master`, later `serial_raw`).

[Back to the index](../builtin_function_doc.md)

---

## Autonics TK temperature controller (`xadp_01_autonics_tk.cpp`)

### `autonics_read(slave_address)`
- **Prototype**: `dict autonics_read(int slave_address)`
- **Return Value**: a `dict` with `success` (`bool`).
  - On success: `pv`, `sv` (floats) and `unit` (string).
  - On failure: `error` (string).
- **Failure Behavior**: returns `{"success": false, "error": "Invalid response"}` or the specific Modbus error text if the read fails.
