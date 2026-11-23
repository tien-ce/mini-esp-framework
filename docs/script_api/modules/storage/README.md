# Module `storage`

Data that survives a restart.

[Back to the API index](../../README.md)

```c
import storage;
```

## Handles

| Create with | Handle | Use it for |
| :--- | :--- | :--- |
| `storage.NVS()` | [`nvs`](nvs.md) | Small settings stored by key (`int`, `float`, `bool`, `string`) |
| `storage.FILE()` | [`file`](file.md) | Writing text files on LittleFS |

## Which one to use

| You want to store | Use |
| :--- | :--- |
| A setpoint, a counter, a calibration value | `nvs` |
| A log or any text that grows | `file` |
| Read or replace a whole file in one call | Global [`file_read` / `file_write`](../../global_functions.md#files) |

## Examples

- [`examples/storage/nvs.ti`](../../../../examples/storage/nvs.ti)
- [`examples/storage/file.ti`](../../../../examples/storage/file.ti)
