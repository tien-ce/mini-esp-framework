# Module `ipc`

Links between scripts running at the same time.

[Back to the API index](../../README.md)

```c
import ipc;
```

## Handles

| Create with | Handle | Use it for |
| :--- | :--- | :--- |
| `ipc.QUEUE(string name, int capacity)` | [`queue`](queue.md) | Send messages between scripts |

## Common pattern: main and workers

A slow job (Modbus, HTTP) goes to a worker script, so the main script is never blocked.

```text
main.ti  --- request queue --->  worker.ti
         <--- reply queue  ---
```

- The worker sleeps in `recv(-1)` until it gets a job.
- The main script starts the worker with [`system.run(path)`](../system/README.md).
- A signal handler may call `send()`, because it never waits.

## Example

[`examples/ipc/worker/`](../../../../examples/ipc/worker)
