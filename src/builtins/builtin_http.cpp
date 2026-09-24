#include "built_in.h"
#include "built_in.h"
#include "TienInterpreter.h"
#include "core/log_task.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

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

/** @brief Built-in http_get function returning a dictionary (code, payload) */
static value_t *built_in_http_get_dict(value_t **argv, int argc) {
    if (argc != 1 || argv == NULL || argv[0] == NULL || argv[0]->type != VAL_STRING || argv[0]->string_val == NULL) {
        ti_log("[ERROR] %s: Expect 1 string argument (url)\n", BUILTIN_HTTP_GET);
        ti_fatal();
    }

    HTTPClient http;
    http.setTimeout(5000); // 5 seconds is safer than 500ms
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

    value_t *dict_val = val_new_dict();
    val_dict_set(dict_val->dict_val, "code", val_new_int(httpCode));
    val_dict_set(dict_val->dict_val, "payload", val_new_string(response.c_str()));
    return dict_val;
}

/** @brief Built-in http_post function returning a dictionary (code, payload) */
static value_t *built_in_http_post_dict(value_t **argv, int argc) {
    if (argc < 2 || argc > 3 || argv == NULL || argv[0] == NULL || argv[1] == NULL ||
        argv[0]->type != VAL_STRING || argv[1]->type != VAL_STRING ||
        argv[0]->string_val == NULL || argv[1]->string_val == NULL) {
        ti_log("[ERROR] %s: Expect 2 or 3 string args (url, data, [content_type])\n", BUILTIN_HTTP_POST);
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

    value_t *dict_val = val_new_dict();
    val_dict_set(dict_val->dict_val, "code", val_new_int(httpCode));
    val_dict_set(dict_val->dict_val, "payload", val_new_string(response.c_str()));
    return dict_val;
}

void builtin_http_init(void) {
    static param_t http_get_params[] = { { VAL_STRING, (char*)"url" } };
    register_builtin_function(BUILTIN_HTTP_GET, VAL_DICT, http_get_params, 1, built_in_http_get_dict);
    register_builtin_function(BUILTIN_HTTP_POST, VAL_DICT, NULL, -1, built_in_http_post_dict);
}
