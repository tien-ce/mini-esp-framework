#include "core/ti_interpreter.h"
#include "core/log_task.h"
#include "core/file_system.h"
#include "core/web/web_ws.h"
#include "built_in.h"
#include "TienInterpreter.h"
#include <LittleFS.h>
#include <HTTPClient.h>
#include <algorithm>
#include <stddef.h>
#include <string.h>
#include <ArduinoJson.h>

/* -------------------------------------------------------------------------- */
/*                              FRAMEWORK BUILTIN FUNCTIONS                   */
/* -------------------------------------------------------------------------- */
static value_t *built_in_is_none(value_t **argv, int argc)
{
    if (argc != 1 || argv == NULL) {
        ti_log("[ERROR] %s: Expect 1 variable \n", BUILTIN_IS_NONE);
        ti_fatal();
    }
    if (argv[0] == NULL || argv[0]->type == VAL_NULL)
    {
      return val_new_bool(true);
    }
    else
    {
      return val_new_bool(false);
    }
}

/** @brief Static helper to validate arguments, parse JSON string, and extract a JsonVariant for a given key. */
static bool parse_json_and_get_element(value_t **argv, int argc, const char *func_name, JsonDocument &doc, JsonVariant &out_variant) {
    if (argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL || argv[0]->type != VAL_STRING || argv[1]->type != VAL_STRING) {
        ti_log("[ERROR] %s: Expect 2 string arguments (json_string, key)\n", func_name);
        ti_fatal();
    }

    const char *json_str = argv[0]->string_val;
    const char *key = argv[1]->string_val;

    if (json_str == NULL || key == NULL) {
        return false;
    }

    DeserializationError err = deserializeJson(doc, json_str);
    if (err) {
        LOG_WARNING(String(func_name) + " parse error: " + String(err.c_str()));
        return false;
    }

    if (!doc.containsKey(key) || doc[key].isNull()) {
        return false;
    }

    out_variant = doc[key];
    return true;
}

/** @brief Built-in get_json function to extract an element from JSON string with its native type. */
static value_t *built_in_get_json(value_t **argv, int argc) {
    JsonDocument doc;
    JsonVariant val;
    if (!parse_json_and_get_element(argv, argc, BUILTIN_GET_JSON, doc, val)) {
        return val_new_null();
    }

    if (val.isNull()) {
        return val_new_null();
    }
    if (val.is<bool>()) {
        return val_new_bool(val.as<bool>());
    }
    if (val.is<int>()) {
        return val_new_int(val.as<int>());
    }
    if (val.is<float>()) {
        return val_new_float(val.as<float>());
    }
    if (val.is<const char*>()) {
        return val_new_string(val.as<const char*>());
    }
    if (val.is<JsonObject>() || val.is<JsonArray>()) {
        String json_out;
        serializeJson(val, json_out);
        return val_new_string(json_out.c_str());
    }

    String val_str = val.as<String>();
    return val_new_string(val_str.c_str());
}

/** @brief Built-in get_json_as_string function to extract an element from JSON string as a string. */
static value_t *built_in_get_json_as_string(value_t **argv, int argc) {
    JsonDocument doc;
    JsonVariant val;
    if (!parse_json_and_get_element(argv, argc, BUILTIN_GET_JSON_AS_STRING, doc, val)) {
        return val_new_null();
    }

    if (val.is<const char*>()) {
        return val_new_string(val.as<const char*>());
    }

    if (val.is<JsonObject>() || val.is<JsonArray>()) {
        String json_out;
        serializeJson(val, json_out);
        return val_new_string(json_out.c_str());
    }

    String val_str = val.as<String>();
    return val_new_string(val_str.c_str());
}

/** @brief Built-in get_json_as_int function to extract an element from JSON string as an integer. */
static value_t *built_in_get_json_as_int(value_t **argv, int argc) {
    JsonDocument doc;
    JsonVariant val;
    if (!parse_json_and_get_element(argv, argc, BUILTIN_GET_JSON_AS_INT, doc, val)) {
        return val_new_null();
    }

    return val_new_int(val.as<int>());
}

/** @brief Built-in get_json_as_float function to extract an element from JSON string as a float. */
static value_t *built_in_get_json_as_float(value_t **argv, int argc) {
    JsonDocument doc;
    JsonVariant val;
    if (!parse_json_and_get_element(argv, argc, BUILTIN_GET_JSON_AS_FLOAT, doc, val)) {
        return val_new_null();
    }

    return val_new_float(val.as<float>());
}

/** @brief Built-in get_json_as_bool function to extract an element from JSON string as a boolean. */
static value_t *built_in_get_json_as_bool(value_t **argv, int argc) {
    JsonDocument doc;
    JsonVariant val;
    if (!parse_json_and_get_element(argv, argc, BUILTIN_GET_JSON_AS_BOOL, doc, val)) {
        return val_new_null();
    }

    return val_new_bool(val.as<bool>());
}
/** @brief Built-in delay function to pause script execution for N milliseconds. */
static value_t *built_in_delay(value_t **argv, int argc) {
    if (argc != 1 || argv == NULL || argv[0] == NULL) {
        ti_log("[ERROR] %s: Invalid argument count, expect 1\n", BUILTIN_DELAY);
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
            ti_log("[ERROR] %s: Unexpected type %d\n", BUILTIN_DELAY, argv[0]->type);
            ti_fatal();
            break;
    }
    return init_val(VAL_NULL);
}

/** @brief Built-in http_get function executing HTTP GET and returning JSON string with code and payload. */
static value_t *built_in_http_get(value_t **argv, int argc) {
    if (argc != 1 || argv == NULL || argv[0] == NULL || argv[0]->type != VAL_STRING || argv[0]->string_val == NULL) {
        ti_log("[ERROR] %s: Expect 1 string argument (url)\n", BUILTIN_HTTP_GET);
        ti_fatal();
    }

    HTTPClient http;
    http.setTimeout(500);
    bool beginOk = http.begin(argv[0]->string_val);
    int httpCode = -1;
    String response = "";

    if (!beginOk) {
        httpCode = -1;
        response = "Failed to parse URL or initialize client";
    } else {
        httpCode = http.GET();
        if (httpCode > 0) {
            response = http.getString();
        } else {
            response = http.errorToString(httpCode);
        }
        http.end();
    }

    JsonDocument doc;
    doc["code"] = httpCode;
    doc["payload"] = response;

    String out;
    serializeJson(doc, out);
    return val_new_string(out.c_str());
}

/** @brief Built-in http_post function executing HTTP POST and returning JSON string with code and payload. */
static value_t *built_in_http_post(value_t **argv, int argc) {
    if (argc < 2 || argc > 3 || argv == NULL || argv[0] == NULL || argv[1] == NULL ||
        argv[0]->type != VAL_STRING || argv[1]->type != VAL_STRING ||
        argv[0]->string_val == NULL || argv[1]->string_val == NULL) {
        ti_log("[ERROR] %s: Expect 2 or 3 string arguments (url, data, [content_type])\n", BUILTIN_HTTP_POST);
        ti_fatal();
    }

    if (argc == 3 && (argv[2] == NULL || argv[2]->type != VAL_STRING || argv[2]->string_val == NULL)) {
        ti_log("[ERROR] %s: Argument 3 must be a valid string (content_type)\n", BUILTIN_HTTP_POST);
        ti_fatal();
    }

    HTTPClient http;
    http.setTimeout(5000);
    bool beginOk = http.begin(argv[0]->string_val);
    int httpCode = -1;
    String response = "";

    if (!beginOk) {
        httpCode = -1;
        response = "Failed to parse URL or initialize client";
    } else {
        const char *contentType = (argc == 3) ? argv[2]->string_val : "application/json";
        http.addHeader("Content-Type", contentType);
        httpCode = http.POST((uint8_t*)argv[1]->string_val, strlen(argv[1]->string_val));
        if (httpCode > 0) {
            response = http.getString();
        } else {
            response = http.errorToString(httpCode);
        }
        http.end();
    }

    JsonDocument doc;
    doc["code"] = httpCode;
    doc["payload"] = response;

    String out;
    serializeJson(doc, out);
    return val_new_string(out.c_str());
}

/* -------------------------------------------------------------------------- */
/*                              STATIC VARIABLES                              */
/* -------------------------------------------------------------------------- */

static std::vector<TI_TASK_STRUCT> g_tien_tasks;

typedef struct {
    char name[32];
    char *source_code;
} TienTaskParam_t;

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief Removes a task from the active task vector by its FreeRTOS task handle. */
static void remove_tien_task_by_handle(TaskHandle_t handle) {
    auto it = std::find_if(g_tien_tasks.begin(), g_tien_tasks.end(),
        [handle](const TI_TASK_STRUCT &task) { return task.handle == handle; });
    if (it == g_tien_tasks.end()) return;
    g_tien_tasks.erase(it);
}

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
    /* Remove TI_TASK_STRUCT from list */
    remove_tien_task_by_handle(xTaskGetCurrentTaskHandle());
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

/** @brief CLI command handler for 'tien_stop <name>' command. */
static void tien_stop_cmd(const String &args) {
    String cleanArg = args;
    cleanArg.trim();
    if (cleanArg.length() == 0) return;
    tien_stop(cleanArg.c_str());
}

/** @brief FreeRTOS task function to execute Tien script in background task. */
static void interpreter_task(void *pvParameters) {
    TienTaskParam_t *param = (TienTaskParam_t *)pvParameters;
    if (param != NULL) {
        if (param->source_code != NULL) {
            ti_run_string(param->source_code);
            UBaseType_t remainingWords = uxTaskGetStackHighWaterMark(NULL);
            LOG_INFO("[Debug] Min Free Stack: " + String(remainingWords * sizeof(StackType_t)) + " bytes");
            free(param->source_code);
        }
        remove_tien_task_by_handle(xTaskGetCurrentTaskHandle());
        free(param);
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
    /* Use path like the name of script task */
    tien_run_script(path, buffer);
    free(buffer);
}

void tien_run_script(const char *name, const char *source_code) {
    if (name == NULL || strlen(name) == 0 || source_code == NULL) {
        ti_log("[ERROR] Invalid script name or source code\n");
        return;
    }

    for (const auto &task : g_tien_tasks) {
        if (strcmp(task.name, name) == 0) {
            ti_log("[ERROR] Task '%s' already exists\n", name);
            return;
        }
    }

    /* Create new param to pass to task, copy name and source code to free independently */
    TienTaskParam_t *param = (TienTaskParam_t *)malloc(sizeof(TienTaskParam_t));
    if (param == NULL) {
        ti_log("[ERROR] Memory allocation failed for task '%s'\n", name);
        return;
    }

    strncpy(param->name, name, sizeof(param->name) - 1);
    param->name[sizeof(param->name) - 1] = '\0';
    param->source_code = strdup(source_code);

    if (param->source_code == NULL) {
        free(param);
        ti_log("[ERROR] Memory allocation failed for task '%s'\n", name);
        return;
    }

    TaskHandle_t task_handle = NULL;
    BaseType_t ret = xTaskCreate(interpreter_task, name, 8192, (void*)param, 3, &task_handle);
    if (ret != pdPASS) {
        /* If task created failed */
        LOG_ERROR("Failed to create interpreter task: " + String(name));
        ti_log("[ERROR] Failed to create interpreter task '%s'\n", name);
        free(param->source_code);
        free(param);
        return;
    }

    TI_TASK_STRUCT task_entry;
    strncpy(task_entry.name, name, sizeof(task_entry.name) - 1);
    task_entry.name[sizeof(task_entry.name) - 1] = '\0';
    task_entry.handle = task_handle;
    g_tien_tasks.push_back(task_entry);
}

void tien_stop(const char *name) {
    if (name == NULL || strlen(name) == 0) return;

    auto it = std::find_if(g_tien_tasks.begin(), g_tien_tasks.end(),
        [name](const TI_TASK_STRUCT &task) { return strcmp(task.name, name) == 0; });

    if (it == g_tien_tasks.end()) {
        ti_log("[ERROR] Task '%s' not found\n", name);
        return;
    }

    TaskHandle_t target_handle = it->handle;
    g_tien_tasks.erase(it);

    if (target_handle != NULL) {
        vTaskDelete(target_handle);
    }
    ti_log("[INFO] Task '%s' stopped successfully\n", name);
    LOG_INFO("Tien task '" + String(name) + "' stopped successfully");
}

void tien_init(void) {
    ti_register_log(tien_log_callback);
    ti_register_fatal(tien_fatal_callback);

    register_builtin_function(BUILTIN_IS_NONE, built_in_is_none);
    register_builtin_function(BUILTIN_DELAY, built_in_delay);
    register_builtin_function(BUILTIN_HTTP_GET, built_in_http_get);
    register_builtin_function(BUILTIN_HTTP_POST, built_in_http_post);
    register_builtin_function(BUILTIN_GET_JSON, built_in_get_json);
    register_builtin_function(BUILTIN_GET_JSON_AS_STRING, built_in_get_json_as_string);
    register_builtin_function(BUILTIN_GET_JSON_AS_INT, built_in_get_json_as_int);
    register_builtin_function(BUILTIN_GET_JSON_AS_FLOAT, built_in_get_json_as_float);
    register_builtin_function(BUILTIN_GET_JSON_AS_BOOL, built_in_get_json_as_bool);

    register_cmd("tien", tien_run_cmd);
    register_cmd("tien_stop", tien_stop_cmd);
}

