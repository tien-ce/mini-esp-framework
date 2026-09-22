#include "built_in.h"
#include "built_in.h"
#include "TienInterpreter.h"
#include "core/file_system.h"
#include <LittleFS.h>
#include <Arduino.h>

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

void builtin_fs_init(void) {
    static param_t fs_path_param[] = { { VAL_STRING, (char*)"path" } };
    static param_t fs_write_params[] = { { VAL_STRING, (char*)"path" }, { VAL_STRING, (char*)"data" } };

    register_builtin_function(BUILTIN_FILE_READ, VAL_STRING, fs_path_param, 1, built_in_file_read);
    register_builtin_function(BUILTIN_FILE_WRITE, VAL_BOOL, fs_write_params, 2, built_in_file_write);
    register_builtin_function(BUILTIN_FILE_EXISTS, VAL_BOOL, fs_path_param, 1, built_in_file_exists);
    register_builtin_function(BUILTIN_FILE_REMOVE, VAL_BOOL, fs_path_param, 1, built_in_file_remove);
}
