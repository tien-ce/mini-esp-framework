#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

/*---- INCLUDES ----*/
#include <stddef.h>
#include <stdint.h>

/*---- ENUMS & TYPES ----*/
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
    FS_ERR_IS_DIRECTORY,     /**< Path exists but is a directory, not a file */
    FS_ERR_NOT_A_DIRECTORY,  /**< Path exists but is a file, not a directory */
    FS_ERR_ALREADY_EXISTS,   /**< Path already exists */
    FS_ERR_OPEN_FAILED,      /**< LittleFS could not open the path */
    FS_ERR_ALLOC_FAILED,     /**< Heap/PSRAM allocation for a read/list buffer failed */
    FS_ERR_WRITE_INCOMPLETE, /**< Fewer bytes were written to flash than requested */
    FS_ERR_OPERATION_FAILED, /**< The underlying LittleFS call failed */
    FS_ERR_INVALID_ARG,      /**< A required argument was NULL */
} FsResult_t;

/*---- PUBLIC FUNCTIONS ----*/

/** 
 * @brief Human-readable description of a FsResult_t.
 * @param result The result code to translate.
 * @return const char* String description of the error.
 */
const char *file_system_strerror(FsResult_t result);

/**
 * @brief Initializes the file system (LittleFS).
 * @param formatonfail True to format the file system if mounting fails.
 * @param basepath The base mount point.
 * @param maxopenfiles Maximum number of concurrently open files.
 * @return true if mounted successfully, false otherwise.
 */
bool file_system_init(bool formatonfail = true, const char *basepath = "/littlefs", uint8_t maxopenfiles = 4);

/**
 * @brief Formats the LittleFS partition.
 * @return true on successful format, false otherwise.
 */
bool file_system_format();

/**
 * @brief Gets the total size of the file system.
 * @return size_t Total bytes available.
 */
size_t file_system_get_size();

/**
 * @brief Gets the used size of the file system.
 * @return size_t Total bytes currently used.
 */
size_t file_system_get_used();

/**
 * @brief Reads the entire contents of a file from LittleFS into a dynamically allocated buffer.
 * 
 * @details Opens the file in read-only mode, calculates its size, and allocates dynamic memory
 *          with size equal to file capacity + 1 byte for the null-terminator '\0'.
 * 
 * @note MEMORY OWNERSHIP CONTRACT:
 *       The caller assumes full ownership of the `*out_data` memory and is strictly responsible
 *       for freeing it via `free()` after use to prevent memory leaks.
 * 
 * @param[in]  path           Absolute path of the file to read on LittleFS.
 * @param[out] out_data       Pointer to the dynamically allocated buffer (caller owns and frees).
 * @param[out] out_bytes_read Pointer to store the actual number of bytes successfully read.
 * @return FsResult_t Status code (FS_OK on success, or corresponding error codes).
 */
FsResult_t read_file(const char *path, char **out_data, unsigned int *out_bytes_read);

/**
 * @brief Writes data to a file in LittleFS.
 * @param path Absolute path of the file.
 * @param data Pointer to the data buffer to write.
 * @param length Number of bytes to write.
 * @param out_bytes_written Pointer to store the actual number of bytes written.
 * @return FsResult_t Status code.
 */
FsResult_t write_file(const char *path, const char *data, unsigned int length, unsigned int *out_bytes_written);

/**
 * @brief Iterates through a directory and serializes the file list into a flat JSON string.
 * 
 * @note MEMORY OWNERSHIP CONTRACT:
 *       The caller assumes full ownership of the `*out_json` memory and is strictly responsible
 *       for freeing it via `free()`.
 * 
 * @param[in]  dir_path The directory path to list.
 * @param[out] out_json Pointer to the dynamically allocated JSON string (caller owns and frees).
 * @return FsResult_t Status code.
 */
FsResult_t list_file(const char *dir_path, char **out_json);

/**
 * @brief Removes a file from the file system.
 * @param path Absolute path of the file.
 * @return FsResult_t Status code.
 */
FsResult_t remove_file(const char *path);

/**
 * @brief Renames a file in the file system.
 * @param pathfrom Absolute path of the existing file.
 * @param pathto Absolute path of the new file destination.
 * @return FsResult_t Status code.
 */
FsResult_t rename_file(const char *pathfrom, const char *pathto);

/**
 * @brief Creates a directory.
 * @param path Absolute path of the directory.
 * @return FsResult_t Status code.
 */
FsResult_t make_directory(const char *path);

/**
 * @brief Removes a directory.
 * @param path Absolute path of the directory.
 * @return FsResult_t Status code.
 */
FsResult_t remove_dir(const char *path);

#endif // FILE_SYSTEM_H
