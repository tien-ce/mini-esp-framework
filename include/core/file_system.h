#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Outcome of a file_system.h operation.
 * Every access function below returns one of these instead of a bare bool/NULL,
 * so callers (e.g. the webserver) can map a failure to the right response
 * without re-checking existence/type themselves beforehand.
 */
typedef enum {
    FS_OK = 0,               /**< Operation completed successfully */
    FS_ERR_NOT_MOUNTED,      /**< file_system_init() was not called, or LittleFS.begin() failed */
    FS_ERR_NOT_FOUND,        /**< Path does not exist */
    FS_ERR_IS_DIRECTORY,     /**< Path exists but is a directory, not a file (e.g. read_file/write_file target) */
    FS_ERR_NOT_A_DIRECTORY,  /**< Path exists but is a file, not a directory (e.g. list_file target) */
    FS_ERR_ALREADY_EXISTS,   /**< Path already exists (e.g. make_directory on an existing path) */
    FS_ERR_OPEN_FAILED,      /**< LittleFS could not open the path (corrupt entry, too many open files, ...) */
    FS_ERR_ALLOC_FAILED,     /**< Heap/PSRAM allocation for a read/list buffer failed */
    FS_ERR_WRITE_INCOMPLETE, /**< Fewer bytes were written to flash than requested */
    FS_ERR_OPERATION_FAILED, /**< The underlying LittleFS call (remove/rename/mkdir/rmdir) failed */
    FS_ERR_INVALID_ARG,      /**< A required argument (path/data/out-pointer) was NULL */
} FsResult_t;

/** @brief Human-readable description of a FsResult_t, for logs or as an HTTP response body. */
const char *file_system_strerror(FsResult_t result);

bool file_system_init(bool formatonfail = true, const char *basepath = "/littlefs", uint8_t maxopenfiles = 4);
bool file_system_format();
size_t file_system_get_size();
size_t file_system_get_used();

/* file and directory interaction */
FsResult_t read_file(const char *path, char **out_data, unsigned int *out_bytes_read);
FsResult_t write_file(const char *path, const char *data, unsigned int length, unsigned int *out_bytes_written);
FsResult_t list_file(const char *dir_path, char **out_json);
FsResult_t remove_file(const char *path);
FsResult_t rename_file(const char *pathfrom, const char *pathto);
FsResult_t make_directory(const char *path);
FsResult_t remove_dir(const char *path);

#endif // !FILE_SYSTEM_H
