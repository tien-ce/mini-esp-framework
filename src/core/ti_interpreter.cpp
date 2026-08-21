#include "core/ti_interpreter.h"
#include "core/log_task.h"
#include "core/file_system.h"
#include "core/web/web_ws.h"
#include "built_in.h"
#include "TienInterpreter.h"
#include <LittleFS.h>
#include <stddef.h>
#include <string.h>
#include <ArduinoJson.h>

/* -------------------------------------------------------------------------- */
/*                              FRAMEWORK BUILTIN FUNCTIONS                   */
/* -------------------------------------------------------------------------- */

/** @brief Built-in get_json_element function to extract an element from JSON string as a string. */
static value_t *built_in_get_json_element(value_t **argv, int argc) {
    if (argc != 2 || argv[0]->type != VAL_STRING || argv[1]->type != VAL_STRING) {
        ti_log("ERROR %s: Expect 2 string arguments (json_string, key)\n", BUILTIN_GET_JSON_ELEMENT);
        ti_fatal();
    }

    const char *json_str = argv[0]->string_val;
    const char *key = argv[1]->string_val;

    if (json_str == NULL || key == NULL) {
        return val_new_null();
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json_str);
    if (err) {
        LOG_WARNING("get_json_element parse error: " + String(err.c_str()));
        return val_new_null();
    }

    if (!doc.containsKey(key) || doc[key].isNull()) {
        return val_new_null();
    }

    if (doc[key].is<const char*>()) {
        return val_new_string(doc[key].as<const char*>());
    }

    if (doc[key].is<JsonObject>() || doc[key].is<JsonArray>()) {
        String json_out;
        serializeJson(doc[key], json_out);
        return val_new_string(json_out.c_str());
    }

    String val_str = doc[key].as<String>();
    return val_new_string(val_str.c_str());
}
/** @brief Built-in print function callback registered into TienInterpreter engine. */
static value_t *built_in_print(value_t **argv, int argc) {
    if (argc == 0) {
        LOG_INFO("\n");
    }
    for (int i = 0; i < argc; i++) {
        switch (argv[i]->type) {
            case VAL_STRING:
                LOG_INFO(String(argv[i]->string_val));
                break;
            case VAL_INT:
                LOG_INFO(String(argv[i]->int_val));
                break;
            case VAL_FLOAT:
                LOG_INFO(String(argv[i]->float_val, 2));
                break;
            default:
                ti_log("ERROR %s: Unexpected type %d\n", BUILTIN_PRINT, argv[i]->type);
                ti_fatal();
                break;
        }
    }
    return init_val(VAL_NULL);
}

/** @brief Built-in delay function to pause script execution for N milliseconds. */
static value_t *built_in_delay(value_t **argv, int argc) {
    if (argc != 1) {
        ti_log("ERROR %s: Invalid argument count, expect 1\n", BUILTIN_DELAY);
        ti_fatal();
    }
    switch (argv[0]->type) {
        case VAL_INT:
            if (argv[0]->int_val > 0) {
                vTaskDelay(pdMS_TO_TICKS(argv[0]->int_val));
            }
            break;
        case VAL_FLOAT:
            if (argv[0]->float_val > 0.0f) {
                vTaskDelay(pdMS_TO_TICKS((int)argv[0]->float_val));
            }
            break;
        default:
            ti_log("ERROR %s: Unexpected type %d\n", BUILTIN_DELAY, argv[0]->type);
            ti_fatal();
            break;
    }
    return init_val(VAL_NULL);
}

/** @brief Built-in file_read function to read entire text file from LittleFS into a string. */
static value_t *built_in_file_read(value_t **argv, int argc) {
    if (argc != 1 || argv[0]->type != VAL_STRING) {
        ti_log("ERROR %s: Expect 1 string argument (file path)\n", BUILTIN_FILE_READ);
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
    if (argc != 2 || argv[0]->type != VAL_STRING || argv[1]->type != VAL_STRING) {
        ti_log("ERROR %s: Expect 2 string arguments (file path, content)\n", BUILTIN_FILE_WRITE);
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
    if (argc != 1 || argv[0]->type != VAL_STRING) {
        ti_log("ERROR %s: Expect 1 string argument (file path)\n", BUILTIN_FILE_EXISTS);
        ti_fatal();
    }
    bool exists = LittleFS.exists(argv[0]->string_val);
    return val_new_bool(exists);
}

/** @brief Built-in file_remove function to delete a file from LittleFS. */
static value_t *built_in_file_remove(value_t **argv, int argc) {
    if (argc != 1 || argv[0]->type != VAL_STRING) {
        ti_log("ERROR %s: Expect 1 string argument (file path)\n", BUILTIN_FILE_REMOVE);
        ti_fatal();
    }
    FsResult_t ret = remove_file(argv[0]->string_val);
    return val_new_bool(ret == FS_OK);
}
/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief Validates if given file path has .ti script file extension. */
static bool has_tien_extension(const char *path) {
    if (path == NULL) return false;
    
    size_t len = strlen(path);
    const char *ext = ".ti";
    size_t ext_len = strlen(ext);
    
    if (len < ext_len) return false;
    
    return strcmp(path + len - ext_len, ext) == 0;
}

/** @brief Logging callback function registered to TienInterpreter to route output to Serial and Web. */
static void tien_log_callback(const char *fmt, va_list args) {
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    Serial.print(buffer);
    tien_out_to_ws(buffer);
}

/** @brief Fatal error callback registered to TienInterpreter to log and terminate task. */
static void tien_fatal_callback(void) {
    LOG_ERROR("Interpreter fatal error occurred!");
    tien_out_to_ws("[Fatal Error]: Interpreter execution halted.\n");
    vTaskDelete(NULL); 
}

/** @brief CLI command handler for 'tien <filepath.ti>' command. */
static void tien_run_cmd(const String &args) {
    String cleanArg = args;
    cleanArg.trim();
    if (cleanArg.length() == 0) return;

    String firstArg;
    int spaceIndex = cleanArg.indexOf(' ');
    if (spaceIndex != -1) {
        firstArg = cleanArg.substring(0, spaceIndex);
    } else {
        firstArg = cleanArg;
    }
    tien_run_file(firstArg.c_str());
}

/** @brief FreeRTOS task function to execute Tien script in background task. */
static void interpreter_task(void *pvParameters) {
    char *buffer = (char *)pvParameters;
    if (buffer != NULL) {
        ti_run_string(buffer);
        UBaseType_t remainingWords = uxTaskGetStackHighWaterMark(NULL);
        LOG_INFO("[Debug] Min Free Stack: " + String(remainingWords * sizeof(StackType_t)) + " bytes");
        free(buffer); 
    }
    vTaskDelete(NULL);
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

void tien_run_file(const char *path) {
    if (!has_tien_extension(path)) {
        LOG_ERROR(String(path) + " is not a .ti file");
        return;
    }
    char *buffer = NULL;
    size_t read_bytes = 0;
    FsResult_t ret = read_file(path, &buffer, &read_bytes);
    if (ret != FS_OK) {
        LOG_ERROR("Read error: " + String(file_system_strerror(ret)));
        return;
    }
    tien_run_script(buffer);
    free(buffer);
}

void tien_run_script(const char *source_code) {
    if (source_code == NULL) return;
    char *task_payload = strdup(source_code);
    if (task_payload == NULL) return;

    BaseType_t ret = xTaskCreate(interpreter_task, "interpreter_task", 8192, (void*)task_payload, 3, NULL);
    if (ret != pdPASS) {
        LOG_ERROR("Failed to create interpreter task");
        free(task_payload);
    }
}

void tien_init(void) {
    ti_register_log(tien_log_callback);
    ti_register_fatal(tien_fatal_callback);

    register_builtin_function(BUILTIN_PRINT, built_in_print);
    register_builtin_function(BUILTIN_DELAY, built_in_delay);
    register_builtin_function(BUILTIN_FILE_READ, built_in_file_read);
    register_builtin_function(BUILTIN_FILE_WRITE, built_in_file_write);
    register_builtin_function(BUILTIN_FILE_EXISTS, built_in_file_exists);
    register_builtin_function(BUILTIN_FILE_REMOVE, built_in_file_remove);
    register_builtin_function(BUILTIN_GET_JSON_ELEMENT, built_in_get_json_element);

    register_cmd("tien", tien_run_cmd);
}

