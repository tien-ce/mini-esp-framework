/**
 * @file file_system.cpp
 * @brief Thin wrapper around LittleFS: mount, read/write/list/remove/rename files and directories.
 *
 * Every public function returns an FsResult_t so callers (webserver, scripts) can map a failure to the
 * right response without checking existence or type themselves. LittleFS is the only source of truth:
 * nothing about the file tree is cached in RAM, every check is done on flash.
 */

/*---- INCLUDES ----*/
#include "core/file_system.h"
#include "config.h"
#include "core/core_log.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <cstring>

/*---- STATIC VARIABLES ----*/
/** @brief Copy of the base path passed to file_system_init(), kept for diagnostics. */
static char *s_basepath = NULL;
/** @brief Indicates whether the file system has been successfully mounted. */
static bool s_fs_mounted = false;

/*---- PUBLIC FUNCTIONS ----*/

/** @brief Human-readable description of a FsResult_t */
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
        case FS_ERR_NOT_EMPTY:        return "Directory is not empty";
        default:                      return "Unknown file system error";
    }
}

/** @brief Initializes (mounts) the file system (LittleFS). */
bool file_system_init(bool formatonfail, const char *basepath, uint8_t maxopenfiles)
{
    // 1. Mount the LittleFS partition.
    s_fs_mounted = LittleFS.begin(formatonfail, basepath, maxopenfiles);
    if (!s_fs_mounted) {
        LOG_ERROR_STR("File system initialized failed, can't mount file system");
        return false;
    }

    // 2. Keep a copy of the base path for diagnostics. Free any previous copy first so calling init()
    //    twice can never leak it.
    free(s_basepath);
    s_basepath = strdup(basepath);
    return true;
}

/** @brief Formats the LittleFS partition. */
bool file_system_format()
{
    return LittleFS.format();
}

/** @brief Gets the total size of the file system */
size_t file_system_get_size()
{
    return LittleFS.totalBytes();
}

/** @brief Gets the used size of the file system */
size_t file_system_get_used()
{
    return LittleFS.usedBytes();
}

/** @brief Reads the entire contents of a file into a dynamically allocated buffer. */
FsResult_t read_file(const char *path, char **out_data, unsigned int *out_bytes_read)
{
    // 1. Initialize out-pointers to safe defaults.
    if (out_data) *out_data = NULL;
    if (out_bytes_read) *out_bytes_read = 0;

    // 2. Validate input pointers and mount state.
    if (!path || !out_data || !out_bytes_read) return FS_ERR_INVALID_ARG;
    if (!s_fs_mounted) return FS_ERR_NOT_MOUNTED;
    if (strcmp(path, "/") == 0) return FS_ERR_IS_DIRECTORY;   // root is always a directory

    // 3. Open read-only (the File closes itself when out of scope).
    File file = LittleFS.open(path, "r", false);
    if (!file) {
        return LittleFS.exists(path) ? FS_ERR_OPEN_FAILED : FS_ERR_NOT_FOUND;
    }
    if (file.isDirectory()) {
        return FS_ERR_IS_DIRECTORY;
    }

    // 4. Allocate size + 1 byte for '\0' (PSRAM when available). The caller owns it and frees it with free().
    size_t size = file.size();
    #ifdef SYSTEM_USES_PSRAM
    char *buffer = (char*) ps_malloc(size + 1);
    #else
    char *buffer = (char*)malloc(size + 1);
    #endif
    if (!buffer) {
        return FS_ERR_ALLOC_FAILED;
    }

    // 5. Read flash data into the buffer and append the null-terminator.
    size_t readLen = file.readBytes(buffer, size);
    buffer[readLen] = '\0';

    *out_data = buffer;
    *out_bytes_read = readLen;
    return FS_OK;
}

/** @brief Writes data to a file in LittleFS (created, or truncated when it exists). */
FsResult_t write_file(const char *path, const char *data, unsigned int length, unsigned int *out_bytes_written)
{
    if (out_bytes_written) *out_bytes_written = 0;

    // 1. Validate inputs and mount state.
    if (!path || !data) return FS_ERR_INVALID_ARG;
    if (!s_fs_mounted) return FS_ERR_NOT_MOUNTED;
    if (strcmp(path, "/") == 0) return FS_ERR_IS_DIRECTORY;

    // 2. Open (create / truncate) with fopen(path, "w"). It fails when the path is a directory, or when the
    //    parent directory is missing (create = false: the library does not create it).
    // NOTE: with this Arduino-ESP32 version, "w" on a directory makes fopen() fail, so writing to a directory
    //       returns FS_ERR_NOT_FOUND here; the isDirectory() check below is kept in case the library changes.
    File file = LittleFS.open(path, "w", false);
    if (!file) return FS_ERR_NOT_FOUND;
    if (file.isDirectory()) return FS_ERR_IS_DIRECTORY;

    // 3. Write.
    size_t bytesWrite = file.write((const uint8_t*)data, length);
    if (out_bytes_written) *out_bytes_written = (unsigned int)bytesWrite;

    return (bytesWrite == length) ? FS_OK : FS_ERR_WRITE_INCOMPLETE;
}

/** @brief Iterates through a directory and serializes the file list into a flat JSON array string. */
FsResult_t list_file(const char *dir_path, char **out_json)
{
    // 1. Initialize *out_json = NULL; validate input parameters and mount state.
    if (out_json) *out_json = NULL;
    if (!dir_path || !out_json) return FS_ERR_INVALID_ARG;
    if (!s_fs_mounted) return FS_ERR_NOT_MOUNTED;

    // 2. Open the directory (closes itself when out of scope); verify it is a directory.
    File root = LittleFS.open(dir_path);
    if (!root) {
        return LittleFS.exists(dir_path) ? FS_ERR_OPEN_FAILED : FS_ERR_NOT_FOUND;
    }
    if (!root.isDirectory()) {
        return FS_ERR_NOT_A_DIRECTORY;
    }

    // 3. Walk the direct children (not recursive) into a JSON array: [{name, size, is_dir}, ...]
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();

    File file = root.openNextFile();
    while (file) {
        JsonObject item = array.add<JsonObject>();

        // Extract basename (LittleFS on ESP32 sometimes includes full path).
        String fullname = file.name();
        int slashIdx = fullname.lastIndexOf('/');
        if (slashIdx >= 0) fullname = fullname.substring(slashIdx + 1);

        item["name"] = fullname;
        item["size"] = file.size();
        item["is_dir"] = file.isDirectory();

        file = root.openNextFile();
    }

    // 4. Serialize into a malloc'ed string; the caller owns it and frees it with free().
    size_t jsonLen = measureJson(doc) + 1;
    char *jsonBuffer = (char*)malloc(jsonLen);
    if (!jsonBuffer) {
        return FS_ERR_ALLOC_FAILED;
    }
    serializeJson(doc, jsonBuffer, jsonLen);

    *out_json = jsonBuffer;
    return FS_OK;
}

/** @brief Removes a file from the file system. */
FsResult_t remove_file(const char *path)
{
    // 1. Validate arguments and state.
    if (!path) return FS_ERR_INVALID_ARG;
    if (!s_fs_mounted) return FS_ERR_NOT_MOUNTED;

    // 2. Must exist and be a file (a directory goes through remove_dir()).
    File file = LittleFS.open(path, "r", false);
    if (!file) return FS_ERR_NOT_FOUND;
    if (file.isDirectory()) return FS_ERR_IS_DIRECTORY;
    file.close();   // close before deleting

    // 3. Remove.
    return LittleFS.remove(path) ? FS_OK : FS_ERR_OPERATION_FAILED;
}

/** @brief Renames a file in the file system. */
FsResult_t rename_file(const char *pathFrom, const char *pathTo)
{
    // Validate arguments and state.
    if (!pathFrom || !pathTo) return FS_ERR_INVALID_ARG;
    if (!s_fs_mounted) return FS_ERR_NOT_MOUNTED;
    if (!LittleFS.exists(pathFrom)) return FS_ERR_NOT_FOUND;

    return LittleFS.rename(pathFrom, pathTo) ? FS_OK : FS_ERR_OPERATION_FAILED;
}

/** @brief Creates a directory. */
FsResult_t make_directory(const char *path)
{
    // 1. Validate arguments and state.
    if (!path) return FS_ERR_INVALID_ARG;
    if (!s_fs_mounted) return FS_ERR_NOT_MOUNTED;

    // 2. A file or directory with this name already exists.
    if (LittleFS.exists(path)) return FS_ERR_ALREADY_EXISTS;

    // 3. The parent must exist and be a directory (mkdir is not recursive).
    //    "/a/b" -> parent "/a"; "/a" -> parent "/" (always present once mounted).
    const char *lastSlash = strrchr(path, '/');
    if (lastSlash && lastSlash != path) {
        String parent = String(path).substring(0, lastSlash - path);
        File parentDir = LittleFS.open(parent.c_str());
        if (!parentDir) return FS_ERR_NOT_FOUND;
        if (!parentDir.isDirectory()) return FS_ERR_NOT_A_DIRECTORY;
    }

    // 4. Create.
    return LittleFS.mkdir(path) ? FS_OK : FS_ERR_OPERATION_FAILED;
}

/** @brief Removes a directory. */
FsResult_t remove_dir(const char *path)
{
    // 1. Validate arguments and state.
    if (!path) return FS_ERR_INVALID_ARG;
    if (!s_fs_mounted) return FS_ERR_NOT_MOUNTED;
    if (strcmp(path, "/") == 0) return FS_ERR_INVALID_ARG;   // the root is never removed

    // 2. Must exist and be a directory (a file goes through remove_file()).
    File dir = LittleFS.open(path, "r", false);
    if (!dir) return FS_ERR_NOT_FOUND;
    if (!dir.isDirectory()) return FS_ERR_NOT_A_DIRECTORY;

    // 3. LittleFS refuses a non-empty directory but only answers false, so look for a child first.
    File child = dir.openNextFile();
    if (child) return FS_ERR_NOT_EMPTY;
    child.close();
    dir.close();    // close before deleting

    // 4. Remove.
    return LittleFS.rmdir(path) ? FS_OK : FS_ERR_OPERATION_FAILED;
}
