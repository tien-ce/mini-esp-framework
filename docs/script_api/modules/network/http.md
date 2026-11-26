# Handle `http`

One HTTP client. It keeps the connection to the same server between requests when the server allows it.

[Back to module `network`](README.md)

```c
import network;

handle http = network.HTTP();
```

## Attributes

| Attribute | Type | Access | Meaning |
| :--- | :--- | :--- | :--- |
| `timeout_ms` | `int` | read / write | Connect timeout and read timeout, `1..65535`. Default `5000` |
| `content_type` | `string` | read / write | `Content-Type` sent by `post` and `post_async`. Default `"application/json"` |
| `code` | `int` | read only | Result of the last request: the HTTP status, or a negative number when the request failed |
| `payload` | `string` | read only | Result of the last request: the answer body, or the reason when `code` is negative |

**No attribute can be read or written while a request runs on this handle.** It raises an error. See [Waiting is not blocking](#waiting-is-not-blocking).

## Methods

| Method | Returns | Description |
| :--- | :--- | :--- |
| `get(string url)` | void | Send a GET and wait until it ended. The result is in `code` and `payload` |
| `post(string url, string body)` | void | Send a POST and wait until it ended. The result is in `code` and `payload` |
| `get_async(string url, func on_done)` | void | Send a GET and go on at once. `on_done(http)` is called when it ended |
| `post_async(string url, string body, func on_done)` | void | Send a POST and go on at once. `on_done(http)` is called when it ended |

One request at a time per handle. Use two handles for two requests at the same time.

## Usage

**Wait for the answer**

```c
http.timeout_ms = 3000;

http.get("http://192.0.2.10/api/temp?ip=192.0.2.20");
if (http.code == 200) {
    print(http.payload);
} else {
    print_error("GET failed, code ", http.code, ": ", http.payload);
}

http.post("http://192.0.2.10/api/log", "{\"cycle\": 1}");
print("POST ", http.code);
```

**With a callback**

The callback gets the handle that made the request. The request has ended when it runs, so it can read `code` and `payload` and even send the next request on the same handle.

```c
void on_done(handle h) {
    print("GET ", h.code, ": ", h.payload);
}

http.get_async("http://192.0.2.10/api/temp", on_done);

// The script must keep running for the callback to be called
while (true) {
    delay(1000);
}
```

## Waiting is not blocking

`get` and `post` wait from the script's point of view only: the next line runs after the request ended. While they wait, the script **still runs its callbacks**: signal handlers, `uart.on_receive` handlers, `on_done` of other requests, and calls from other scripts. A callback can therefore run in the middle of `http.get(...)`.

**Do not use a handle in a callback while the main code may be waiting on that same handle.** The handle is busy, so in the callback:

- `get`, `post`, `get_async`, `post_async` raise "a request is already running on this handle"
- reading or writing any attribute raises

Use a separate handle in the callback, or only set a variable there and do the request in the main loop.

**Exception:** the `on_done` callback of `get_async` / `post_async` runs after its request ended, so using its own handle there is safe.

[`modbus.request`](../hardware/modbus.md#waiting-is-not-blocking) waits in the same way.

## Errors

| Case | Result |
| :--- | :--- |
| A method is called while a request runs on this handle | Raises |
| An attribute is read or written while a request runs on this handle | Raises |
| `timeout_ms` out of `1..65535`, or an attribute set to the wrong type | Raises |
| `code` or `payload` is written | Raises (read only) |
| No Wi-Fi connection | `code` = `-1`, `payload` = `"Network Disconnected"`, at once |
| URL cannot be parsed | `code` = `-1`, `payload` = `"Failed to parse URL or initialize client"` |
| No answer, connection refused or lost | `code` < `0`, `payload` = the reason, for example `"read Timeout"` or `"connection refused"` |
| Out of memory for the request task | `code` = `-1`, `payload` = `"Cannot start the HTTP request task"` |
| Server answered with an error | `code` = the HTTP status (`400`, `404`, `500`, ...), `payload` = its body |

When the script is stopped during `get` or `post`, the stop takes effect once the request ended, at most about twice `timeout_ms` (connect and read).

## Example

[`examples/network/http/`](../../../../examples/network/http): `config.ti` holds the URLs and the timeout, `main.ti` runs a GET, a POST and a `get_async`.
