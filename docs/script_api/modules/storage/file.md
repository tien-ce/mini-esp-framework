# Handle `file`

One LittleFS file kept open between writes. To read a file, use the global [`file_read`](../../global_functions.md#files).

[Back to module `storage`](README.md)

```c
import storage;

handle log_file = storage.FILE();    // created closed
```

## Methods

| Method | Returns | Description |
| :--- | :--- | :--- |
| `open(string path, string mode)` | void | Open the file. `mode` is passed to LittleFS |
| `write(string data)` | `int` | Write at the current position. Returns the bytes written |
| `flush()` | void | Push buffered data to flash |
| `close()` | void | Close the file, buffered data is written |

| Mode | Meaning |
| :---: | :--- |
| `"r"` | Read |
| `"w"` | Create or replace the file |
| `"a"` | Create the file or add to its end |

## Usage

```c
handle log_file = storage.FILE();
log_file.open("/log.txt", "a");
log_file.write("started\n");
log_file.close();
```

- Data is only safe on flash after `flush()` or `close()`.
- A missing parent folder is not created.
- If `write` returns less than the length of the data, the write failed, for example because the flash is full.
- At most 4 files can be open at once across the **whole firmware**, the web server included. Close a file when you are done.

## Errors

| Case | Result |
| :--- | :--- |
| The file cannot be opened | `open` raises |
| `open` on an already open `file` | Raises |
| `write`, `flush` or `close` on a closed `file` | Raises |

## Example

[`examples/storage/file.ti`](../../../../examples/storage/file.ti)
