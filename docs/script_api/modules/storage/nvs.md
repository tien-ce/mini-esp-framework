# Handle `nvs`

One namespace of the ESP32 non-volatile storage, for small values stored by key.

[Back to module `storage`](README.md)

```c
import storage;

handle nvs = storage.NVS();      // created closed
```

## Methods

| Method | Returns | Description |
| :--- | :--- | :--- |
| `open(string namespace)` | `bool` | Open read/write. The namespace is created when it does not exist |
| `write(string key, value)` | `int` | Store an `int`, `float`, `bool` or `string`. Returns the bytes written, `0` on failure |
| `read(string key, default)` | type of `default` | The stored value, or `default` when the key does not exist |
| `write_dict(dict values)` | void | Store every entry, the key of the dict is the NVS key |
| `read_dict(dict values)` | void | Replace each value by the one stored under its key |
| `close()` | void | Close the namespace |

Namespace names and keys are limited to 15 characters.

## Usage

```c
handle nvs = storage.NVS();
if (nvs.open("settings")) {
    nvs.write("max_temp", 90);
    int max_temp = nvs.read("max_temp", 60);
    print(max_temp);
    nvs.close();
}
```

`default` also sets the type that is read, so read a key with the same type it was written with.

`read_dict` uses the values of the dict as defaults, and changes the dict you pass:

```c
dict settings = {"max_temp": 60, "name": "line_1"};
nvs.read_dict(settings);         // a key not stored keeps its value
```

## Errors

| Case | Result |
| :--- | :--- |
| Any method but `open` on a closed `nvs`, `open` on an already open one | Raises |
| `write` value or `read` default is not `int`, `float`, `bool` or `string` | Raises |
| Namespace cannot be opened, for example a name that is too long | `open` returns `false` |
| Write failure | `write` returns `0` |
| An entry of `write_dict` or `read_dict` cannot be handled | Logged as a warning and skipped |

## Example

[`examples/storage/nvs.ti`](../../../../examples/storage/nvs.ti)
