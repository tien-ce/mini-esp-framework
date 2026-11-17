/**
 * @file file_handle.cpp
 * @brief Script handle "file": methods and destructor of file_value_t.
 */

/*---- INCLUDES ----*/
#include "scripting/handle/storage/file_handle.h"
#include <LittleFS.h>
#include <string.h>

/*---- STATIC FUNCTION PROTOTYPES ----*/

static void file_destroy(void *ptr);

static value_t *file_open(ti_handle_t handle, value_t **argv, int argc);
static value_t *file_write(ti_handle_t handle, value_t **argv, int argc);
static value_t *file_flush(ti_handle_t handle, value_t **argv, int argc);
static value_t *file_close(ti_handle_t handle, value_t **argv, int argc);

/*---- PARAMS ----*/

/** open(path, mode). params[0] is self: the method is found through the handle's type, so it is always a file. */
static const param_t s_file_open_params[]  = { PARAM_HANDLE("self", NULL), PARAM_STRING("path"), PARAM_STRING("mode"), };
/** write(data). */
static const param_t s_file_write_params[] = { PARAM_HANDLE("self", NULL), PARAM_STRING("data"), };
/** flush(), close(). */
static const param_t s_file_self_params[]  = { PARAM_HANDLE("self", NULL), };

/*---- METHODS ----*/

static const ti_builtin_def_t s_file_methods[] = {
    { "open",  VAL_VOID, s_file_open_params,  3, file_open  },
    { "write", VAL_INT,  s_file_write_params, 2, file_write },
    { "flush", VAL_VOID, s_file_self_params,  1, file_flush },
    { "close", VAL_VOID, s_file_self_params,  1, file_close },
};

/*---- HANDLE TYPE ----*/

/* External linkage comes from the extern declaration in file_handle.h */
const ti_handle_type_t g_file_handle_type = {
    "file",                                                         /* name */
    file_destroy,                                                   /* destroy */
    s_file_methods,                                                 /* methods */
    (int)(sizeof(s_file_methods) / sizeof(s_file_methods[0])),      /* method_count */
    NULL,                                                           /* get_attr: no attributes */
    NULL,                                                           /* set_attr: no attributes */
};

/*---- STATIC FUNCTIONS: METHODS ----*/

/**
 * @brief Method file.open(string path, string mode): open path with mode, passed as is to LittleFS.
 * @param handle Calling runtime handle, used to raise errors.
 * @param argv Arguments: (self, path, mode). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Void; NULL after raising an error (already open, or LittleFS could not open the path).
 */
static value_t *file_open(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    file_value_t *self = (file_value_t *)argv[0]->handle_val->ptr;
    const char *path   = argv[1]->string_val;
    const char *mode   = argv[2]->string_val;

    // Step 1: this handle already holds a file (close it first)
    if (self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "file.open: a file is already open on this handle, close it first");
        return NULL;
    }

    // Step 2: open; create = false, so a missing parent directory is not created
    self->file = LittleFS.open(path, mode, false);
    if (!self->file) {
        ti_raise_error(handle, TI_ERR_RUNTIME, "file.open: cannot open the path with this mode");
        return NULL;
    }
    self->is_open = true;
    return val_new_void();
}

/**
 * @brief Method file.write(string data): write data at the current position.
 * @param handle Calling runtime handle, used to raise errors.
 * @param argv Arguments: (self, data). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Int: bytes written (less than the length of data on failure, e.g. flash full); NULL after raising an error.
 */
static value_t *file_write(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    file_value_t *self = (file_value_t *)argv[0]->handle_val->ptr;
    const char *data   = argv[1]->string_val;

    if (!self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "file.write: not open, call open(path, mode) first");
        return NULL;
    }
    size_t written = self->file.write((const uint8_t *)data, strlen(data));
    return val_new_int((int)written);
}

/**
 * @brief Method file.flush(): push the data buffered by the library to flash.
 * @param handle Calling runtime handle, used to raise errors.
 * @param argv Arguments: (self). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Void; NULL after raising an error.
 */
static value_t *file_flush(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    file_value_t *self = (file_value_t *)argv[0]->handle_val->ptr;

    if (!self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "file.flush: not open, call open(path, mode) first");
        return NULL;
    }
    self->file.flush();
    return val_new_void();
}

/**
 * @brief Method file.close(): close the file; the handle can be opened again afterwards.
 * @param handle Calling runtime handle, used to raise errors.
 * @param argv Arguments: (self). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Void; NULL after raising an error.
 */
static value_t *file_close(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    file_value_t *self = (file_value_t *)argv[0]->handle_val->ptr;

    if (!self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "file.close: file is not open");
        return NULL;
    }
    self->file.close();
    self->is_open = false;
    return val_new_void();
}

/*---- STATIC FUNCTIONS: DESTRUCTOR ----*/

/**
 * @brief Destructor of the file handle type: runs when the last value pointing to the file is freed
 *        (out of scope, script ended or stopped). Closes the file if still open (buffered data is written),
 *        then deletes the object.
 * @param ptr The file_value_t, allocated with new by storage.FILE() (never NULL when called).
 */
static void file_destroy(void *ptr) {
    file_value_t *self = (file_value_t *)ptr;
    if (self->is_open) {
        self->file.close();
    }
    delete self;
}
