#include "core/file_system.h"
#include "built_in.h"
#include "TienInterpreter.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Tracks whether LittleFS.begin() succeeded, so every access function below
// can fail fast with FS_ERR_NOT_MOUNTED instead of letting LittleFS surface
// its own generic "not mounted" error deep inside open()/exists().
static bool g_fs_mounted = false;

/* -------------------------------------------------------------------------- */
/*                        INTERPRETER BUILT-IN FUNCTIONS                      */
/* -------------------------------------------------------------------------- */

/** @brief Built-in file_read function to read entire text file from LittleFS into a string. */
static value_t *built_in_file_read(value_t **argv, int argc) {
    if (argc != 1 || argv == NULL || argv[0] == NULL || argv[0]->type != VAL_STRING || argv[0]->string_val == NULL) {
        ti_log("[ERROR] %s: Expect 1 string argument (file path)\n", BUILTIN_FILE_READ);
        ti_fatal();
    }
    char *buffer = NULL;
    size_t read_bytes = 0;
    FsResult_t ret = read_file(argv[0]->string_val, &buffer, &read_bytes);
    if (ret != FS_OK || buffer == NULL) {
        return val_new_null();
    }
    value_t *result = val_new_string(buffer);
    free(buffer);
    return result;
}

/** @brief Built-in file_write function to overwrite content to a LittleFS file. */
static value_t *built_in_file_write(value_t **argv, int argc) {
    if (argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL ||
        argv[0]->type != VAL_STRING || argv[1]->type != VAL_STRING ||
        argv[0]->string_val == NULL || argv[1]->string_val == NULL) {
        ti_log("[ERROR] %s: Expect 2 string arguments (file path, content)\n", BUILTIN_FILE_WRITE);
        ti_fatal();
    }
    const char *path = argv[0]->string_val;
    const char *data = argv[1]->string_val;
    unsigned int written = 0;
    FsResult_t ret = write_file(path, data, (unsigned int)strlen(data), &written);
    return val_new_bool(ret == FS_OK);
}

/** @brief Built-in file_exists function to check if a file exists on LittleFS. */
static value_t *built_in_file_exists(value_t **argv, int argc) {
    if (argc != 1 || argv == NULL || argv[0] == NULL || argv[0]->type != VAL_STRING || argv[0]->string_val == NULL) {
        ti_log("[ERROR] %s: Expect 1 string argument (file path)\n", BUILTIN_FILE_EXISTS);
        ti_fatal();
    }
    bool exists = LittleFS.exists(argv[0]->string_val);
    return val_new_bool(exists);
}

/** @brief Built-in file_remove function to delete a file from LittleFS. */
static value_t *built_in_file_remove(value_t **argv, int argc) {
    if (argc != 1 || argv == NULL || argv[0] == NULL || argv[0]->type != VAL_STRING || argv[0]->string_val == NULL) {
        ti_log("[ERROR] %s: Expect 1 string argument (file path)\n", BUILTIN_FILE_REMOVE);
        ti_fatal();
    }
    FsResult_t ret = remove_file(argv[0]->string_val);
    return val_new_bool(ret == FS_OK);
}

const char *file_system_strerror(FsResult_t result)
{
    switch (result) {
        case FS_OK:                   return "OK";
        case FS_ERR_NOT_MOUNTED:      return "File system is not mounted";
        case FS_ERR_NOT_FOUND:        return "File or directory not found";
        case FS_ERR_IS_DIRECTORY:     return "Path is a directory, not a file";
        case FS_ERR_NOT_A_DIRECTORY:  return "Path is a file, not a directory";
        case FS_ERR_ALREADY_EXISTS:   return "Path already exists";
        case FS_ERR_OPEN_FAILED:      return "Failed to open path";
        case FS_ERR_ALLOC_FAILED:     return "Memory allocation failed";
        case FS_ERR_WRITE_INCOMPLETE: return "Not all bytes were written";
        case FS_ERR_OPERATION_FAILED: return "File system operation failed";
        case FS_ERR_INVALID_ARG:      return "Invalid argument";
        default:                      return "Unknown file system error";
    }
}

bool file_system_init(bool formatonfail, const char *basepath, uint8_t maxopenfiles)
{
    g_fs_mounted = LittleFS.begin(formatonfail, basepath, maxopenfiles);
    register_builtin_function(BUILTIN_FILE_READ, built_in_file_read);
    register_builtin_function(BUILTIN_FILE_WRITE, built_in_file_write);
    register_builtin_function(BUILTIN_FILE_EXISTS, built_in_file_exists);
    register_builtin_function(BUILTIN_FILE_REMOVE, built_in_file_remove);
    return g_fs_mounted;
}

bool file_system_format()
{
    return LittleFS.format();
}

size_t file_system_get_size()
{
    return LittleFS.totalBytes();
}
size_t file_system_get_used()
{
    return LittleFS.usedBytes();
}

/* file and directory interaction */
FsResult_t read_file(const char *path, char **out_data, unsigned int *out_bytes_read)
{
    if (out_data) *out_data = NULL;
    if (out_bytes_read) *out_bytes_read = 0;
    if (!path || !out_data || !out_bytes_read) return FS_ERR_INVALID_ARG;
    if (!g_fs_mounted) return FS_ERR_NOT_MOUNTED;

    File file = LittleFS.open(path, "r", false);
    if (!file) {
        // open() doesn't say *why* it failed - only pay for exists() to disambiguate on failure.
        return LittleFS.exists(path) ? FS_ERR_OPEN_FAILED : FS_ERR_NOT_FOUND;
    }
    if (file.isDirectory()) {
        file.close();
        return FS_ERR_IS_DIRECTORY;
    }

    size_t size = file.size();
    #ifdef SYSTEM_USES_PSRAM
    char *buffer = (char*) ps_malloc(size + 1);
    #else
    char *buffer = (char*)malloc(size + 1);
    #endif
    if (!buffer) {
        file.close();
        return FS_ERR_ALLOC_FAILED;
    }

    size_t readLen = file.readBytes(buffer, size);
    buffer[readLen] = '\0';
    file.close();

    *out_data = buffer;
    *out_bytes_read = readLen;
    return FS_OK;
}

FsResult_t write_file(const char *path, const char *data, unsigned int length, unsigned int *out_bytes_written)
{
    if (out_bytes_written) *out_bytes_written = 0;
    if (!path || !data) return FS_ERR_INVALID_ARG;
    if (!g_fs_mounted) return FS_ERR_NOT_MOUNTED;

    File file = LittleFS.open(path, "w", false);
    if (!file) return FS_ERR_OPEN_FAILED;
    if (file.isDirectory()) {
        file.close();
        return FS_ERR_IS_DIRECTORY;
    }

    size_t bytesWrite = file.write((const uint8_t*)data, length);
    file.close();

    if (out_bytes_written) *out_bytes_written = (unsigned int)bytesWrite;
    return (bytesWrite == length) ? FS_OK : FS_ERR_WRITE_INCOMPLETE;
}

FsResult_t list_file(const char *dir_path, char **out_json)
{
    if (out_json) *out_json = NULL;
    if (!dir_path || !out_json) return FS_ERR_INVALID_ARG;
    if (!g_fs_mounted) return FS_ERR_NOT_MOUNTED;

    File root = LittleFS.open(dir_path);
    if (!root) {
        return LittleFS.exists(dir_path) ? FS_ERR_OPEN_FAILED : FS_ERR_NOT_FOUND;
    }
    if (!root.isDirectory()) {
        root.close();
        return FS_ERR_NOT_A_DIRECTORY;
    }

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            obj[file.name()] = file.size();
        }
        file = root.openNextFile();
    }
    root.close();

    size_t jsonLen = measureJson(doc) + 1;
    char *jsonBuffer = (char*)malloc(jsonLen);
    if (!jsonBuffer) {
        return FS_ERR_ALLOC_FAILED;
    }
    serializeJson(doc, jsonBuffer, jsonLen);
    *out_json = jsonBuffer;
    return FS_OK;
}

FsResult_t remove_file(const char *path)
{
    if (!path) return FS_ERR_INVALID_ARG;
    if (!g_fs_mounted) return FS_ERR_NOT_MOUNTED;
    if (!LittleFS.exists(path)) return FS_ERR_NOT_FOUND;
    return LittleFS.remove(path) ? FS_OK : FS_ERR_OPERATION_FAILED;
}

FsResult_t rename_file(const char *pathFrom, const char *pathTo)
{
    if (!pathFrom || !pathTo) return FS_ERR_INVALID_ARG;
    if (!g_fs_mounted) return FS_ERR_NOT_MOUNTED;
    if (!LittleFS.exists(pathFrom)) return FS_ERR_NOT_FOUND;
    return LittleFS.rename(pathFrom, pathTo) ? FS_OK : FS_ERR_OPERATION_FAILED;
}

FsResult_t make_directory(const char *path)
{
    if (!path) return FS_ERR_INVALID_ARG;
    if (!g_fs_mounted) return FS_ERR_NOT_MOUNTED;
    if (LittleFS.exists(path)) return FS_ERR_ALREADY_EXISTS;
    return LittleFS.mkdir(path) ? FS_OK : FS_ERR_OPERATION_FAILED;
}

FsResult_t remove_dir(const char *path)
{
    if (!path) return FS_ERR_INVALID_ARG;
    if (!g_fs_mounted) return FS_ERR_NOT_MOUNTED;
    if (!LittleFS.exists(path)) return FS_ERR_NOT_FOUND;
    return LittleFS.rmdir(path) ? FS_OK : FS_ERR_OPERATION_FAILED;
}
