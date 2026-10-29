# File System Built-ins (LittleFS)

Source: `src/scripting/builtins/builtin_fs.cpp`.

[Back to the index](../builtin_function_doc.md)

---

### `file_read(path)`
- **Prototype**: `string file_read(string path)`
- **Return Value**: the file contents.
- **Failure Behavior**: returns `null` if the file does not exist or LittleFS is unmounted. Use `is_none(content)` to check.

### `file_write(path, data)`
- **Prototype**: `bool file_write(string path, string data)`
- **Return Value**: `true` on success.
- **Failure Behavior**: returns `false` if LittleFS is unmounted, the flash is full or the path is invalid.

### `file_exists(path)`
- **Prototype**: `bool file_exists(string path)`
- **Return Value**: `true` if the file exists, `false` if it does not or LittleFS is unmounted.

### `file_remove(path)`
- **Prototype**: `bool file_remove(string path)`
- **Return Value**: `true` if deleted, `false` if the file was not found or cannot be deleted.
