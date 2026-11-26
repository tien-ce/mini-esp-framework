/**
 * @file http_handle.cpp
 * @brief Script handle "http" (module network): attributes, methods and destructor of http_handle_t, and
 *        http_task, the task created for each request.
 *
 * Flow of a sync request (http.get / http.post):
 *
 *     script task (native)                          http_task (one per request)
 *     --------------------                          ---------
 *     fill self->method, url, body, runtime
 *     create http_task(self)             ----->     run the request, write code and payload
 *     ti_wait_flag(&self->done)                     ti_signal_flag(runtime, &done): last access to self
 *       (signal handlers keep running)
 *     return void                        <-----     delete itself
 *     (script reads http.code, http.payload)
 *
 * If the script is stopped while waiting, ti_wait_flag returns at once and the native keeps polling done, so it
 * only returns once http_task has left self. The destructor runs after the native returned, so it never frees
 * a self that http_task still uses.
 */

/*---- INCLUDES ----*/
#include "scripting/handle/network/http_handle.h"
#include <freertos/task.h>
#include "services/wifi/wifi_state.h"

/*---- CONFIG ----*/

#define HTTP_TASK_STACK_SIZE   8192    /* Bytes, stack of each http_task */
#define HTTP_TASK_PRIORITY     1
#define HTTP_STOP_POLL_MS      10      /* Poll period for done when the script is stopped mid request */

/*---- STATIC FUNCTION PROTOTYPES ----*/

static value_t *http_request_sync(ti_handle_t handle, http_handle_t *self, HttpMethod_t method,
                                  const char *url, const char *body);
static value_t *http_request_async(ti_handle_t handle, const value_t *self_value, HttpMethod_t method,
                                   const char *url, const char *body, const value_t *on_done);
static void http_run(http_handle_t *self);
static void http_task(void *arg);
static void http_destroy(void *ptr);

static value_t *http_get(ti_handle_t handle, value_t **argv, int argc);
static value_t *http_post(ti_handle_t handle, value_t **argv, int argc);
static value_t *http_get_async(ti_handle_t handle, value_t **argv, int argc);
static value_t *http_post_async(ti_handle_t handle, value_t **argv, int argc);
static ti_status_t http_get_attr(void *ptr, const char *name, value_t **out);
static ti_status_t http_set_attr(void *ptr, const char *name, const value_t *value);

/*---- PARAMS ----*/

/** get(url). params[0] is self: the method is found through the handle's type, so it is always an http. */
static const param_t s_http_get_params[]  = { PARAM_HANDLE("self", NULL), PARAM_STRING("url"), };
/** post(url, body). The Content-Type comes from the attribute content_type. */
static const param_t s_http_post_params[] = { PARAM_HANDLE("self", NULL), PARAM_STRING("url"), PARAM_STRING("body"), };
/** get_async(url, on_done). on_done(http) is called when the request ended. */
static const param_t s_http_get_async_params[]  = { PARAM_HANDLE("self", NULL), PARAM_STRING("url"),
                                                    PARAM_FUNC("on_done"), };
/** post_async(url, body, on_done). on_done(http) is called when the request ended. */
static const param_t s_http_post_async_params[] = { PARAM_HANDLE("self", NULL), PARAM_STRING("url"),
                                                    PARAM_STRING("body"), PARAM_FUNC("on_done"), };

/*---- METHODS ----*/

static const ti_builtin_def_t s_http_methods[] = {
    { "get",        VAL_VOID, s_http_get_params,        2, http_get        },
    { "post",       VAL_VOID, s_http_post_params,       3, http_post       },
    { "get_async",  VAL_VOID, s_http_get_async_params,  3, http_get_async  },
    { "post_async", VAL_VOID, s_http_post_async_params, 4, http_post_async },
};

/*---- HANDLE TYPE ----*/

/* External linkage comes from the extern declaration in http_handle.h */
const ti_handle_type_t g_http_handle_type = {
    "http",                                                         /* name */
    http_destroy,                                                   /* destroy */
    s_http_methods,                                                 /* methods */
    (int)(sizeof(s_http_methods) / sizeof(s_http_methods[0])),      /* method_count */
    http_get_attr,                                                  /* get_attr */
    http_set_attr,                                                  /* set_attr */
};

/*---- STATIC FUNCTIONS: HELPERS ----*/

/**
 * @brief Common part of get() and post(): hand the request to http_task and wait for its result.
 * @details The script waits with ti_wait_flag, so its signal handlers and events keep running meanwhile.
 * @param handle Calling runtime handle (woken by http_task, also used to raise errors).
 * @param self The http handle.
 * @param method GET or POST.
 * @param url Full URL (copied into self).
 * @param body POST body, "" for GET (copied into self).
 * @return Void value, the result is in self->code and self->payload; NULL after raising an error.
 */
static value_t *http_request_sync(ti_handle_t handle, http_handle_t *self, HttpMethod_t method,
                                  const char *url, const char *body) {
    // Step 1: one request at a time per handle (a handle can reach another script through a queue)
    if (__atomic_exchange_n(&self->busy, true, __ATOMIC_ACQ_REL)) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "http: a request is already running on this handle");
        return NULL;
    }

    // Step 2: copy the request into self: argv is freed when this native returns, http_task may still run then
    self->method  = method;
    self->url     = url;
    self->body    = body;
    self->runtime    = handle;
    self->on_done    = NULL;    // sync
    self->self_value = NULL;
    self->done       = false;

    // Step 3: start the task that runs this request
    if (xTaskCreate(http_task, "http_task", HTTP_TASK_STACK_SIZE, self, HTTP_TASK_PRIORITY, NULL) != pdPASS) {
        self->code    = -1;
        self->payload = "Cannot start the HTTP request task";
        __atomic_store_n(&self->busy, false, __ATOMIC_RELEASE);
        return ti_native_check_alloc(handle, val_new_void(), "http");
    }

    // Step 4: wait for done. No limit here: http_task always ends the request within its timeouts
    if (ti_wait_flag(handle, &self->done, TI_PORT_WAIT_FOREVER) != TI_OK) {
        // Stopped: ti_wait_flag returns at once, so keep waiting here. http_task never touches self after
        // setting done, so self stays valid until this native returns
        while (!__atomic_load_n(&self->done, __ATOMIC_ACQUIRE)) {
            vTaskDelay(pdMS_TO_TICKS(HTTP_STOP_POLL_MS));
        }
        __atomic_store_n(&self->busy, false, __ATOMIC_RELEASE);
        return ti_native_check_alloc(handle, val_new_void(), "http");
    }

    // Step 5: the result stays in self->code and self->payload, read by the script as attributes
    __atomic_store_n(&self->busy, false, __ATOMIC_RELEASE);
    return ti_native_check_alloc(handle, val_new_void(), "http");
}

/**
 * @brief Common part of get_async() and post_async(): hand the request to http_task and return at once.
 * @details http_task posts on_done(http) when the request ended; the script reads code and payload there.
 * @param handle Calling runtime handle (receives the on_done event, also used to raise errors).
 * @param self_value The http handle value (argv[0]); copied so the handle stays alive while http_task runs.
 * @param method GET or POST.
 * @param url Full URL (copied into self).
 * @param body POST body, "" for GET (copied into self).
 * @param on_done Callback function value (copied into self).
 * @return Void value; NULL after raising an error.
 */
static value_t *http_request_async(ti_handle_t handle, const value_t *self_value, HttpMethod_t method,
                                   const char *url, const char *body, const value_t *on_done) {
    http_handle_t *self = (http_handle_t *)self_value->handle_val->ptr;

    // Step 1: one request at a time per handle
    if (__atomic_exchange_n(&self->busy, true, __ATOMIC_ACQ_REL)) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "http: a request is already running on this handle");
        return NULL;
    }

    // Step 2: own copies: the callback, and the handle value so the handle stays alive while http_task runs
    value_t *on_done_copy    = val_copy(on_done);
    value_t *self_value_copy = val_copy(self_value);
    if (on_done_copy == NULL || self_value_copy == NULL) {
        val_free(on_done_copy);
        val_free(self_value_copy);
        __atomic_store_n(&self->busy, false, __ATOMIC_RELEASE);
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "http: out of memory");
        return NULL;
    }

    // Step 3: copy the request into self
    self->method     = method;
    self->url        = url;
    self->body       = body;
    self->runtime    = handle;
    self->on_done    = on_done_copy;
    self->self_value = self_value_copy;

    // Step 4: start the task that runs this request
    if (xTaskCreate(http_task, "http_task", HTTP_TASK_STACK_SIZE, self, HTTP_TASK_PRIORITY, NULL) != pdPASS) {
        self->on_done    = NULL;
        self->self_value = NULL;
        val_free(on_done_copy);
        val_free(self_value_copy);    // argv[0] still holds the handle, so this never frees it
        self->code    = -1;
        self->payload = "Cannot start the HTTP request task";
        __atomic_store_n(&self->busy, false, __ATOMIC_RELEASE);
        return ti_native_check_alloc(handle, val_new_void(), "http");
    }

    // Step 5: return at once, on_done(http) is posted when the request ended
    return ti_native_check_alloc(handle, val_new_void(), "http");
}

/*---- STATIC FUNCTIONS: HTTP TASK ----*/

/**
 * @brief Run the request stored in self and hand the result to the script. Runs in http_task only.
 * @details Sync (on_done == NULL): sets done and wakes the waiting script, never touches self afterwards.
 *          Async: posts on_done(http) and frees on_done; a script that has ended is handled by ti_post_event.
 *          Posting can free the handle, so it is the last access to self.
 * @param self Handle whose request fields were filled by the native.
 */
static void http_run(http_handle_t *self) {
    // Step 1: run the request. Without Wi-Fi fail at once instead of waiting for the client's timeouts
    if (!is_wifi_sta_connected()) {
        self->code    = -1;
        self->payload = "Network Disconnected";
    } else {
        // Connect and read timeouts both bounded, so the request always ends
        self->client.setConnectTimeout((int32_t)self->timeout_ms);
        self->client.setTimeout((uint16_t)self->timeout_ms);

        if (!self->client.begin(self->url)) {
            self->code    = -1;
            self->payload = "Failed to parse URL or initialize client";
        } else {
            if (self->method == HTTP_METHOD_POST) {
                self->client.addHeader("Content-Type", self->content_type);
                self->code = self->client.POST(self->body);
            } else {
                self->code = self->client.GET();
            }
            self->payload = (self->code > 0) ? self->client.getString() : HTTPClient::errorToString(self->code);

            // Keeps the TCP connection open when the server allows keep-alive
            self->client.end();
        }
    }

    // Step 2: hand the result to the script
    if (self->on_done == NULL) {
        // Sync: last access to self. The native may return as soon as done is set, so self is never touched after
        ti_signal_flag(self->runtime, &self->done);
    } else {
        // Async: take everything out of self first, then free the handle for the next request
        ti_handle_t runtime    = self->runtime;
        value_t    *on_done    = self->on_done;
        value_t    *self_value = self->self_value;
        self->on_done    = NULL;
        self->self_value = NULL;
        __atomic_store_n(&self->busy, false, __ATOMIC_RELEASE);

        // ti_post_event takes self_value (also on failure) and drops it when the script has ended: that can be
        // the last value of the handle, so self may be freed here and is never touched after
        value_t *args[1] = { self_value };
        ti_post_event(runtime, on_done, args, 1);

        // on_done is only borrowed by ti_post_event and is not part of self
        val_free(on_done);
    }
}

/**
 * @brief http_task: run one request, then delete itself.
 * @param arg The http_handle_t* whose request fields were filled by the native.
 */
static void http_task(void *arg) {
    http_run((http_handle_t *)arg);
    vTaskDelete(NULL);
}

/*---- STATIC FUNCTIONS: METHODS ----*/

/**
 * @brief Method http.get(string url): send a GET request and wait for the answer.
 * @param handle Calling runtime handle.
 * @param argv Arguments: (self, url). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Void value, the result is in the attributes code and payload; NULL after raising an error.
 */
static value_t *http_get(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    http_handle_t *self = (http_handle_t *)argv[0]->handle_val->ptr;
    return http_request_sync(handle, self, HTTP_METHOD_GET, argv[1]->string_val, "");
}

/**
 * @brief Method http.post(string url, string body): send a POST request (Content-Type from the attribute
 *        content_type) and wait for the answer.
 * @param handle Calling runtime handle.
 * @param argv Arguments: (self, url, body). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Void value, the result is in the attributes code and payload; NULL after raising an error.
 */
static value_t *http_post(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    http_handle_t *self = (http_handle_t *)argv[0]->handle_val->ptr;
    return http_request_sync(handle, self, HTTP_METHOD_POST, argv[1]->string_val, argv[2]->string_val);
}

/**
 * @brief Method http.get_async(string url, func on_done): send a GET request and return at once.
 *        on_done(http) is called when the request ended; it reads the attributes code and payload.
 * @param handle Calling runtime handle.
 * @param argv Arguments: (self, url, on_done). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Void value; NULL after raising an error.
 */
static value_t *http_get_async(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    return http_request_async(handle, argv[0], HTTP_METHOD_GET, argv[1]->string_val, "", argv[2]);
}

/**
 * @brief Method http.post_async(string url, string body, func on_done): send a POST request (Content-Type from
 *        the attribute content_type) and return at once. on_done(http) is called when the request ended.
 * @param handle Calling runtime handle.
 * @param argv Arguments: (self, url, body, on_done). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Void value; NULL after raising an error.
 */
static value_t *http_post_async(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    return http_request_async(handle, argv[0], HTTP_METHOD_POST, argv[1]->string_val, argv[2]->string_val, argv[3]);
}

/*---- STATIC FUNCTIONS: ATTRIBUTES ----*/

/**
 * @brief Read http.timeout_ms, http.content_type, http.code or http.payload.
 * @details code and payload hold the result of the last request. No attribute can be read while a request runs.
 * @param ptr The http_handle_t (never NULL: a closed handle is rejected before the call).
 * @param name Attribute name written in the script.
 * @param out Receives a new value owned by the caller (only on TI_OK).
 * @return TI_OK, TI_ERR_INVALID_VALUE during a request, TI_ERR_UNDEFINED for an unknown name, or TI_ERR_NO_MEMORY.
 */
static ti_status_t http_get_attr(void *ptr, const char *name, value_t **out) {
    const http_handle_t *self = (const http_handle_t *)ptr;

    if (__atomic_load_n(&self->busy, __ATOMIC_ACQUIRE)) return TI_ERR_INVALID_VALUE;

    if (strcmp(name, "timeout_ms") == 0) {
        *out = val_new_int((int)self->timeout_ms);
    } else if (strcmp(name, "content_type") == 0) {
        *out = val_new_string(self->content_type.c_str());
    } else if (strcmp(name, "code") == 0) {
        *out = val_new_int(self->code);
    } else if (strcmp(name, "payload") == 0) {
        *out = val_new_string(self->payload.c_str());
    } else {
        return TI_ERR_UNDEFINED;
    }
    return (*out != NULL) ? TI_OK : TI_ERR_NO_MEMORY;
}

/**
 * @brief Write http.timeout_ms or http.content_type. code and payload are read only.
 * @details timeout_ms must fit HTTPClient::setTimeout, which takes a uint16_t: 1 .. 65535.
 *          No attribute can be written while a request runs.
 * @param ptr The http_handle_t (never NULL: a closed handle is rejected before the call).
 * @param name Attribute name written in the script.
 * @param value New value (borrowed: copied into self).
 * @return TI_OK, TI_ERR_INVALID_VALUE during a request, TI_ERR_TYPE_MISMATCH, TI_ERR_VALUE_OUT_OF_RANGE,
 *         TI_ERR_READ_ONLY for code / payload, or TI_ERR_UNDEFINED for an unknown name.
 */
static ti_status_t http_set_attr(void *ptr, const char *name, const value_t *value) {
    http_handle_t *self = (http_handle_t *)ptr;

    if (__atomic_load_n(&self->busy, __ATOMIC_ACQUIRE)) return TI_ERR_INVALID_VALUE;

    if (strcmp(name, "timeout_ms") == 0) {
        if (value->type != VAL_INT) return TI_ERR_TYPE_MISMATCH;
        if (value->int_val < 1 || value->int_val > UINT16_MAX) return TI_ERR_VALUE_OUT_OF_RANGE;
        self->timeout_ms = (uint32_t)value->int_val;
        return TI_OK;
    }
    if (strcmp(name, "content_type") == 0) {
        if (value->type != VAL_STRING) return TI_ERR_TYPE_MISMATCH;
        self->content_type = value->string_val;
        return TI_OK;
    }
    if (strcmp(name, "code") == 0 || strcmp(name, "payload") == 0) {
        return TI_ERR_READ_ONLY;
    }
    return TI_ERR_UNDEFINED;
}

/*---- STATIC FUNCTIONS: DESTRUCTOR ----*/

/**
 * @brief Destructor of the http handle type: runs when the last value pointing to the http is freed
 *        (out of scope, script ended or stopped, or the on_done event dropped).
 * @details Never runs during a request: a sync request returns only after http_task has left self, and an
 *          async request holds its own copy of the handle value until it posts on_done.
 * @param ptr The http_handle_t, allocated with new by network.HTTP() (never NULL when called).
 */
static void http_destroy(void *ptr) {
    http_handle_t *self = (http_handle_t *)ptr;
    delete self;    // ~HTTPClient closes a kept-alive connection
}
