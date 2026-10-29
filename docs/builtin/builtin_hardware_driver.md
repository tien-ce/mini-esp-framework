# Hardware Driver Built-ins

Built-ins registered by the on-board drivers (`src/hardware/driver/`).

[Back to the index](../builtin_function_doc.md)

---

## Relay (`xdrv_02_relay.cpp`)

### `relay_get_state(relay_num)`
- **Prototype**: `int relay_get_state(int relay_num)`
- **Return Value**: `1` (ON) or `0` (OFF).
- **Failure Behavior**: returns `0` if the relay is inactive; halts with `ti_fatal()` if `relay_num` is out of range (e.g. < 1 or > 3).

### `relay_set_state(relay_num, state)`
- **Prototype**: `bool relay_set_state(int relay_num, <int/bool/string> state)`
- **Return Value**: `true` on success.
- **Failure Behavior**: returns `false` if the relay is inactive; halts with `ti_fatal()` on an invalid argument.
