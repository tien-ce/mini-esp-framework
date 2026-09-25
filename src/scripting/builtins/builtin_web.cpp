#include <Arduino.h>
#include "built_in.h"
#include "TienInterpreter.h"
#include "core/core_log.h"
#include "services/web/web_template.h"
#include <HTTPClient.h>
#include "core/core_engine.h"

/**
 * @brief Update the display value on the Web Template UI.
 * Polymorphism support: automatically converts int, float, bool, string to C++ String 
 * before calling the system function updateElementValue.
 */
static value_t *built_in_web_ui_update(value_t **argv, int argc) {
    if (argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL) {
        ti_log("[ERROR] web_ui_update: Expect 2 args (string element_id, value)\n");
        ti_fatal();
    }
    
    if (argv[0]->type != VAL_STRING) {
        ti_log("[ERROR] web_ui_update: element_id must be a string\n");
        ti_fatal();
    }

    String element_id = argv[0]->string_val ? argv[0]->string_val : "";
    String value_str = "";

    switch (argv[1]->type) {
        case VAL_INT:
            value_str = String(argv[1]->int_val);
            break;
        case VAL_FLOAT:
            value_str = String(argv[1]->float_val);
            break;
        case VAL_BOOL:
            value_str = argv[1]->bool_val ? "true" : "false";
            break;
        case VAL_STRING:
            value_str = argv[1]->string_val ? String(argv[1]->string_val) : "";
            break;
        default:
            ti_log("[ERROR] web_ui_update: Unsupported value type. Allowed: int, float, bool, string\n");
            ti_fatal();
    }

    updateElementValue(element_id, value_str);
    return val_new_bool(true);
}


/** @brief Built-in http_get function returning a dictionary (code, payload) */
static value_t *built_in_web_http_get_dict(value_t **argv, int argc) {
    if (argc != 1 || argv == NULL || argv[0] == NULL || argv[0]->type != VAL_STRING || argv[0]->string_val == NULL) {
        ti_log("[ERROR] %s: Expect 1 string argument (url)\n", BUILTIN_HTTP_GET);
        ti_fatal();
    }

    if (CoreState_GetNetwork() != NET_STATE_WIFI_STA) {
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
static value_t *built_in_web_http_post_dict(value_t **argv, int argc) {
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

    if (CoreState_GetNetwork() != NET_STATE_WIFI_STA) {
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

void builtin_web_init(void) {
    /* 
     * Set param_count = -1 and params = NULL to bypass static type checking,
     * allowing the second parameter (value) to accept any primitive data type (Polymorphism)
     * strictly adhering to the TI_INTERPRETER_GUIDE.md documentation.
     */
    register_builtin_function("web_ui_update", VAL_BOOL, NULL, -1, built_in_web_ui_update);

    static param_t http_get_params[] = { { VAL_STRING, (char*)"url" } };
    register_builtin_function(BUILTIN_HTTP_GET, VAL_DICT, http_get_params, 1, built_in_web_http_get_dict);
    register_builtin_function(BUILTIN_HTTP_POST, VAL_DICT, NULL, -1, built_in_web_http_post_dict);
}
