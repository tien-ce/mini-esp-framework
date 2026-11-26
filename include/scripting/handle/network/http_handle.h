#ifndef HTTP_HANDLE_H
#define HTTP_HANDLE_H

/**
 * @file http_handle.h
 * @brief Script handle "http" (module network): one HTTP client kept for the lifetime of the handle.
 *
 * Layering:
 *
 *     HTTPClient  ->  http handle  ->  http_task (runs the request)  ->  TI script (network.HTTP())
 *
 * The request runs in http_task, a task created for each request, not in the script:
 *   - sync  (on_done == NULL): the script waits with ti_wait_flag, its signal handlers and events keep running
 *   - async (on_done set):     the script goes on, on_done(http) is posted with ti_post_event
 *
 * One request at a time per handle. The handle is never freed while http_task uses it:
 *   - sync:  the native returns only after http_task has left the handle
 *   - async: http_task holds its own copy of the handle value until it posts on_done
 *
 * Script view:
 *   attributes: timeout_ms     int, default 5000
 *               content_type   string, Content-Type of post(), default "application/json"
 *               code           int, result of the last request
 *               payload        string, result of the last request
 *   methods:    get(url)                       -> void, result in code and payload
 *               post(url, body)                -> void, result in code and payload
 *               get_async(url, on_done)        -> void, on_done(http) called when the request ended
 *               post_async(url, body, on_done) -> void, on_done(http) called when the request ended
 *   code is the HTTP status, or a negative number when the request failed (payload holds the reason).
 */

/*---- INCLUDES ----*/
#include <Arduino.h>
#include <HTTPClient.h>
#include "TienInterpreter.h"

/*---- TYPES ----*/

/** @brief HTTP method of a request. */
typedef enum {
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
} HttpMethod_t;

/**
 * @brief Native object wrapped by an "http" handle value.
 * @note Holds C++ objects (HTTPClient, String): allocate with new and free with delete, never calloc / free.
 */
typedef struct {
    HTTPClient        client;         /**< Kept between requests: the connection to the same server is reused. */
    bool              busy;           /**< Set when a request is queued, cleared once its result was taken
                                           (by the native for sync, by http_task for async). */
    uint32_t          timeout_ms;     /**< Attribute timeout_ms, given to the client before each request. */
    String            content_type;   /**< Attribute content_type, sent with every POST. */

    /* Request: written by the native before it hands the handle to http_task */
    HttpMethod_t      method;         /**< GET or POST. */
    String            url;            /**< Full URL. */
    String            body;           /**< POST body (empty for GET). */
    ti_handle_t       runtime;        /**< Script that made the request: woken (sync) or posted to (async). */
    value_t          *on_done;        /**< Async: own copy of the callback (val_copy). NULL: sync. Freed by http_task. */
    value_t          *self_value;     /**< Async: own copy of this handle's value, keeps the handle alive while
                                           http_task runs. Given to on_done as its argument. */

    /* Result: written by http_task */
    bool              done;           /**< Sync: set when the request ended, waited with ti_wait_flag. */
    int               code;           /**< HTTP status, or a negative HTTPClient error code. */
    String            payload;        /**< Response body, or the error text when code < 0. */
} http_handle_t;

/*---- HANDLE TYPE ----*/

/**
 * @brief Handle type of an http value: name "http", destructor, methods and attributes.
 * @details Wrap a new'ed http_handle_t with val_new_handle(ptr, &g_http_handle_type). The value then owns it:
 *          the destructor deletes it when the last value is freed.
 */
extern const ti_handle_type_t g_http_handle_type;

#endif // HTTP_HANDLE_H
