#include "scripting/builtins/built_in.h"
#include "services/wifi/wifi_state.h"
#include "TienInterpreter.h"
#include "core/core_log.h"
#include <HTTPClient.h>
#include "core/core_engine.h"

/** @brief Built-in http_get function returning a dictionary (code, payload) */
static value_t *built_in_http_get_dict(ti_handle_t handle, value_t **argv, int argc) {
    /* (string url) is checked by the interpreter from http_get_params */
    if (!is_wifi_sta_connected()) {
        value_t *dict_val = val_new_dict();
        val_dict_set(dict_val->dict_val, "code", val_new_int(-1));
        val_dict_set(dict_val->dict_val, "payload", val_new_string("Network Disconnected"));
        return dict_val;
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
static value_t *built_in_http_post_dict(ti_handle_t handle, value_t **argv, int argc) {
    if (argc < 2 || argc > 3) {
        ti_raise_error(handle, TI_ERR_INVALID_ARG, "%s: expect (url, data, [content_type]), got %d args", BUILTIN_HTTP_POST, argc);
        return NULL;
    }

    for (int i = 0; i < argc; i++) {
        if (argv[i]->type != VAL_STRING) {
            ti_raise_error(handle, TI_ERR_TYPE_MISMATCH, "%s: argument %d must be a string", BUILTIN_HTTP_POST, i + 1);
            return NULL;
        }
    }

    if (!is_wifi_sta_connected()) {
        value_t *dict_val = val_new_dict();
        val_dict_set(dict_val->dict_val, "code", val_new_int(-1));
        val_dict_set(dict_val->dict_val, "payload", val_new_string("Network Disconnected"));
        return dict_val;
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
