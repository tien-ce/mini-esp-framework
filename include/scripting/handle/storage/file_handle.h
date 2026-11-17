#ifndef FILE_HANDLE_H
#define FILE_HANDLE_H

/**
 * @file file_handle.h
 * @brief Script handle "file": one LittleFS file used by a TI script, built directly on the Arduino File.
 *
 * Layering:
 *
 *     LittleFS (mounted once by file_system_init())  ->  File  ->  file handle  ->  TI script (storage.FILE())
 *
 * LittleFS is a single global object, so the handle does not hold it: it holds the opened File. The file stays
 * open between open() and close(), and is closed when the last value pointing to the handle is destroyed (end
 * of script included).
 *
 * Script view:
 *   methods: open(path, mode)  -> mode passed as is to LittleFS ("r", "w", "a", ...); raises when it fails
 *            write(data)       -> int, bytes written (string)
 *            flush()           -> push buffered data to flash (data is only safe on flash after flush / close)
 *            close()
 *   Calling write / flush / close on a file that is not open raises an error.
 *
 * @note LittleFS is mounted with a limit of open files shared with the whole firmware (web server included,
 *       maxopenfiles of file_system_init(), 4 by default). Every open handle takes one until close().
 */

/*---- INCLUDES ----*/
#include <Arduino.h>
#include <FS.h>
#include "TienInterpreter.h"

/*---- TYPES ----*/

/**
 * @brief Native object wrapped by a "file" handle value.
 * @note Holds a C++ object (File): allocate with new and free with delete, never calloc / free.
 */
typedef struct {
    fs::File file;      /**< Opened file, valid while is_open. */
    bool     is_open;   /**< True between open() and close() / destroy. */
} file_value_t;

/*---- HANDLE TYPE ----*/

/**
 * @brief Handle type of a file value: name "file", destructor, methods.
 * @details Wrap a new'ed file_value_t with val_new_handle(ptr, &g_file_handle_type). The value then owns it:
 *          the destructor closes the file if still open and deletes it when the last value is freed.
 */
extern const ti_handle_type_t g_file_handle_type;

#endif // FILE_HANDLE_H
