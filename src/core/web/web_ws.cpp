#include "core/web/web_ws.h"
#include "core/web_server_task.h"
#include "core/log_task.h"
#include "core/ti_interpreter.h"
#include "config.h"
#include <ArduinoJson.h>

/* -------------------------------------------------------------------------- */
/*                              STRUCTURES & TYPES                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief Context structure for accumulating chunked/separated WebSocket payloads.
 * Allocates buffer in PSRAM (if available) or heap, and tracks total vs received bytes.
 */
struct WsTienUploadContext_t {
    char *buffer = NULL;         /**< Pointer to payload buffer in PSRAM/Heap */
    size_t total_size = 0;       /**< Expected total size of the message */
    size_t received_size = 0;    /**< Currently accumulated bytes count */

    // Deconstructor (need to supprot by delete of cpp)
    ~WsTienUploadContext_t() {
        if (buffer != NULL) {
            free(buffer);
            buffer = NULL;
        }
    }
};

#ifdef SYSTEM_USES_PSRAM
/** @brief Custom ArduinoJson allocator routing JSON document allocations to PSRAM. */
struct PsramAllocator : ArduinoJson::Allocator {
    void* allocate(size_t size) override {
        return ps_malloc(size);
    }
    void deallocate(void* ptr) override {
        free(ptr);
    }
    void* reallocate(void* ptr, size_t new_size) override {
        return ps_realloc(ptr, new_size);
    }
};
static PsramAllocator psramAlloc;
#endif

/* -------------------------------------------------------------------------- */
/*                              GLOBAL VARIABLES                              */
/* -------------------------------------------------------------------------- */

AsyncWebSocket ws("/ws");
AsyncWebSocket ws_tien("/ws_tien");

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief WebSocket event handler for "/ws" terminal console endpoint. */
static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                      void *arg, uint8_t *data, size_t len) {
    // Logic: Handle client connected event
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WebSocket client #%u connected\n", client->id());
        client->text("=== Sensor Monitor Connected ===");
        return;
    } 

    // Logic: Handle client disconnected event
    if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        return;
    } 

    // Logic: Handle incoming data from web terminal
    if (type != WS_EVT_DATA) {
        return;
    }

    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (!info || !info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT) {
        return;
    }

    String msgStr = "";
    for (size_t i = 0; i < len; i++) {
        msgStr += (char)data[i];
    }
    msgStr.trim();
    if (msgStr.length() > 0) {
        postIncomingCommand(msgStr);
    }
}

/** @brief WebSocket event handler for "/ws_tien" Tien script console endpoint. */
static void onWsTienEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                          void *arg, uint8_t *data, size_t len) {
    // Logic: Handle client connection event and send greeting
    if (type == WS_EVT_CONNECT) {
        Serial.printf("Tien WebSocket client #%u connected\n", client->id());
        client->text("=== Tien Script Interpreter Console Connected ===");
        return;
    } 

    // Logic: Handle client disconnect event and clean up any pending chunked buffer
    if (type == WS_EVT_DISCONNECT) {
        Serial.printf("Tien WebSocket client #%u disconnected\n", client->id());
        // If an in-flight chunked upload was interrupted by disconnect, free accumulated buffer
        if (client->_tempObject != NULL) {
            delete (WsTienUploadContext_t*)client->_tempObject;
            client->_tempObject = NULL;
        }
        return;
    } 

    // Logic: Only process incoming WebSocket data frames
    if (type != WS_EVT_DATA) {
        return;
    }

    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (!info || info->opcode != WS_TEXT) {
        return;
    }

    WsTienUploadContext_t *ctx = (WsTienUploadContext_t*)client->_tempObject;

    // Logic: If this is the initial chunk (index == 0), allocate a new accumulation buffer
    if (info->index == 0) {
        // If a leftover context exists from a previous uncompleted payload, free it first
        if (ctx != NULL) {
            delete ctx;
            client->_tempObject = NULL;
        }

        ctx = new WsTienUploadContext_t();
        ctx->total_size = info->len;

        // Allocate buffer in PSRAM when available to conserve internal SRAM for scripts
        #ifdef SYSTEM_USES_PSRAM
        ctx->buffer = (char*)ps_malloc(info->len + 1);
        #else
        ctx->buffer = (char*)malloc(info->len + 1);
        #endif

        // Check if payload buffer allocation in PSRAM/Heap failed
        if (ctx->buffer == NULL) {
            delete ctx;
            client->_tempObject = NULL;
            client->text("[Error]: Memory allocation failed for incoming script payload");
            return;
        }
        client->_tempObject = ctx;
    }

    // Guard against null context or buffer
    if (ctx == NULL || ctx->buffer == NULL) {
        return;
    }

    // Logic: Copy the current chunk into the accumulation buffer at offset info->index
    memcpy(ctx->buffer + info->index, data, len);
    ctx->received_size += len;

    // Logic: Return early if not all chunks have been collected yet
    if (!info->final || (info->index + len != info->len)) {
        return;
    }

    // Null-terminate the full payload string
    ctx->buffer[info->len] = '\0';

    // Parse collected JSON payload
    #ifdef SYSTEM_USES_PSRAM
    JsonDocument doc(&psramAlloc);
    #else
    JsonDocument doc;
    #endif

    DeserializationError error = deserializeJson(doc, (const char*)ctx->buffer, ctx->total_size);
    
    // Logic: Check if payload is valid JSON (structured command from web console)
    if (!error) {
        const char *action = doc["action"] | "run"; // If not valid, return the string after |
        const char *name = doc["name"] | "console";

        // Logic: Branch based on requested action (run script vs stop task)
        if (strcmp(action, "stop") == 0) {
            web_ws_tien_send("> Stop task: " + String(name));
            tien_stop(name);
        } else {
            const char *code = doc["code"] | "";
            web_ws_tien_send("> [" + String(name) + "]\n" + String(code));
            tien_run_script(name, code);
        }
    } else {
        // Logic: Fallback for raw text script input if not JSON formatted
        web_ws_tien_send("> " + String(ctx->buffer));
        tien_run_script("console", ctx->buffer);
    }

    // Free the accumulation context immediately after payload is processed
    delete ctx;
    client->_tempObject = NULL;
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

void web_ws_init(AsyncWebServer *server) {
    if (server == nullptr) return;
    ws.setAuthentication(getWebUsername().c_str(), getWebPassword().c_str());
    ws.onEvent(onWsEvent);
    server->addHandler(&ws);

    ws_tien.setAuthentication(getWebUsername().c_str(), getWebPassword().c_str());
    ws_tien.onEvent(onWsTienEvent);
    server->addHandler(&ws_tien);
}

void web_ws_cleanup() {
    ws.cleanupClients();
    ws_tien.cleanupClients();
}

void web_ws_tien_send(const String &msg) {
    if (ws_tien.count() > 0) {
        ws_tien.textAll(msg);
    }
}

extern "C" void tien_out_to_ws(const char *text) {
    if (text != nullptr) {
        web_ws_tien_send(String(text));
    }
}
