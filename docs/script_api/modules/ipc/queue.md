# Handle `queue`

A named message queue shared by scripts.

[Back to module `ipc`](README.md)

```c
import ipc;

handle jobs = ipc.QUEUE("jobs", 4);
```

Unlike port handles, a queue is ready to use as soon as it is created and has no `open` or `close`.

## `ipc.QUEUE(string name, int capacity)`

- The first script to use a name creates the queue. Other scripts get the same queue by using the same name.
- `name`: 1 to 31 characters.
- `capacity`: 1 to 16 messages. It only counts when the queue is created.
- At most 8 queues exist at the same time, across all scripts.

## Methods

| Method | Returns | Description |
| :--- | :--- | :--- |
| `send(msg)` | `bool` | Put a copy of `msg` (any type) in the queue. **Never waits**: `false` when full |
| `recv(int timeout_ms)` | the message, or `null` | Take the oldest message |

| `timeout_ms` | `recv` behavior |
| :---: | :--- |
| `> 0` | Wait up to this long |
| `0` | Do not wait |
| `< 0` | Wait until a message comes |

## Usage

```c
// Script A
handle jobs = ipc.QUEUE("jobs", 4);
if (jobs.send({"id": 1, "value": 25}) == false) {
    print_warning("Queue full");
}

// Script B
handle jobs = ipc.QUEUE("jobs", 4);
dict job = jobs.recv(1000);
if (job == null) {
    print_warning("No job");
} else {
    print(job["id"]);
}
```

- `send` stores a **deep copy**, so changing the message afterwards does not affect the queue.
- `recv` also returns `null` when the script is being stopped.
- Events and signal handlers keep running while a script waits in `recv`.
- With several scripts waiting, the one that started waiting first gets the next message.
- When the last handle to a queue is released, messages nobody received are dropped.

## Errors

| Case | Result |
| :--- | :--- |
| Invalid `name` or `capacity` | `ipc.QUEUE` raises |
| All 8 queues in use | `ipc.QUEUE` raises |
| Queue full | `send` returns `false` and logs a warning |
| Nothing received in time | `recv` returns `null` |

## Example

[`examples/ipc/worker/`](../../../../examples/ipc/worker)
