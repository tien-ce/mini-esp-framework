# Dispatcher Built-ins

Subscribe a script to every firmware signal (central event dispatcher).

Source: `src/scripting/builtins/builtin_dispatcher.cpp`. Full description, signal list and examples: [`SCRIPT_DISPATCHER.md`](../../SCRIPT_DISPATCHER.md).

[Back to the index](../builtin_function_doc.md)

---

### `dispatcher_register(handler)`
- **Prototype**: `null dispatcher_register(func handler)`
- **Parameters**: `handler` (`func`): script function called as `handler(int signal)` for every signal.
- **Return Value**: `null`.
- **Behavior**:
  - Each script keeps at most ONE handler; registering again replaces it.
  - Signals dispatched synchronously (`SIG_WEB_POLL`, `SIG_MQTT_PUBLISH`) call the handler with `ti_call` and the dispatching task waits for it (timeout 200 ms).
  - Every other signal is queued with `ti_post_event` (fire and forget, in order).
  - A handler that no longer exists or has the wrong parameters is removed with a warning in the log.
- **Failure Behavior**: raises `TI_ERR_INVALID_ARG` when the handler name is empty or longer than 31 characters.

### `dispatcher_unregister()`
- **Prototype**: `null dispatcher_unregister()`
- **Return Value**: `null`. Removes the subscription of the calling script (no effect when it has none).

> `SIG_10MS` floods subscribed scripts: filter it out inside the handler.
