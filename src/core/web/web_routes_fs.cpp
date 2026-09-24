#include "config.h"
#include "core/web/web_routes_fs.h"
#include "core/web/web_auth.h"
#include "core/log_task.h"
#include "core/file_system.h"
#include "hal/uart_types.h"
#include <ArduinoJson.h>
#include <WCharacter.h>

struct UploadContext {
    char *buffer = NULL;
    size_t total_size = 0;
    size_t received_size = 0;

    // Deconstructor (need to supprot by delete of cpp)
    ~UploadContext() {
        if (buffer != NULL)
        {
            free(buffer);
            buffer = NULL;
        }
    }
};

// Custom Allocator forcing all internal allocations to PSRAM
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
// Global or static instance of the allocator
PsramAllocator psramAlloc;
/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief Ensures a path coming from a request (query param or JSON body) is absolute, since LittleFS/VFS paths must start with '/'. */
static String normalize_fs_path(const String &rawPath) {
    if (rawPath.startsWith("/")) {
        return rawPath;
    }
    return "/" + rawPath;
}

/** @brief Maps a file_system.h FsResult_t to the appropriate HTTP status code and sends it. */
static void send_fs_error(AsyncWebServerRequest *request, FsResult_t result) {
    int httpCode;
    switch (result) {
        case FS_ERR_NOT_FOUND:
            httpCode = 404;
            break;
        case FS_ERR_IS_DIRECTORY:
        case FS_ERR_NOT_A_DIRECTORY:
        case FS_ERR_ALREADY_EXISTS:
        case FS_ERR_INVALID_ARG:
            httpCode = 400;
            break;
        case FS_ERR_NOT_MOUNTED:
        case FS_ERR_OPEN_FAILED:
        case FS_ERR_ALLOC_FAILED:
        case FS_ERR_WRITE_INCOMPLETE:
        case FS_ERR_OPERATION_FAILED:
        default:
            httpCode = 500;
            break;
    }
    request->send(httpCode, "text/plain", file_system_strerror(result));
}

/**
 * @brief Accumulates chunked POST body data into a heap buffer stored on request->_tempObject.
 *
 * Shared by /api/fs/save and /api/fs/delete, both of which receive a JSON body that may
 * arrive split across several TCP chunks. Allocates on the first chunk (index == 0) and
 * registers an idempotent(safe if call multiple time) onDisconnect cleanup (safe even if the caller already deleted
 * the context after a normal completion, since it re-reads request->_tempObject instead
 * of a captured pointer).
 */
static UploadContext *accumulate_body_chunk(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    UploadContext *ctx = (UploadContext*)request->_tempObject;
    if (index == 0) {
        ctx = new UploadContext();
        ctx->total_size = total;
        #ifdef SYSTEM_USES_PSRAM
            ctx->buffer = (char*)ps_malloc(total + 1);
        #else
            ctx->buffer = (char*)malloc(total + 1);
        #endif
        if (!ctx->buffer) {
            delete ctx;
            request->_tempObject = nullptr;
            return nullptr;
        }
        request->_tempObject = ctx; // Assign temp_object for next body handles of this request
        request->onDisconnect([request]() {
            UploadContext *pending = (UploadContext*)request->_tempObject;
            // Check if the _tempObject not NULL, the previous called will set to NULL
            // So this is called idempotent
            if (pending) {
                delete pending;
                request->_tempObject = nullptr;
            }
        });
    }

    if (ctx && ctx->buffer) {
        memcpy(ctx->buffer + index, data, len);
        ctx->received_size += len;

        // Null-terminate at the end of the payload
        if (index + len == total) {
            ctx->buffer[total] = '\0';
        }
    }
    return ctx;
}

/**
 * @brief Handles HTTP GET request ("/api/fs/list") to provide LittleFS size and file list.
 * 
 * @details Maps the data structure according to the API_FS_SPEC.md communication contract:
 *          - Queries total capacity (totalBytes) and used capacity (usedBytes).
 *          - Calls list_file("/") to get a raw JSON string as a flat dictionary {"filename": size, ...}.
 *          - Extracts and transforms the structure into an array of objects: files: [ { "name": ..., "size": ... }, ... ].
 *          - Adheres to memory ownership contract: frees the rawListJson buffer via free() immediately after parsing.
 * 
 * @param[in] request Pointer to AsyncWebServerRequest object from client.
 */
static void handleFSList(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;

    JsonDocument doc;
    doc["totalBytes"] = file_system_get_size();
    doc["usedBytes"]  = file_system_get_used();

    // Initialize 'files' array according to API_FS_SPEC.md
    JsonArray filesArray = doc["files"].to<JsonArray>();

    /*
     * MEMORY SAFETY & JSON MAPPING BASED ON API_FS_SPEC.md:
     * 1. Memory ownership contract: list_file() dynamically allocates rawListJson on the Heap.
     *    Caller is strictly responsible for freeing this string via free() after use.
     * 2. Transformation/Mapping Mechanism:
     *    - String returned from list_file() is a flat JSON: {"/file1.txt": 120, "/file2.txt": 85}
     *    - Web Frontend UI expects format according to API_FS_SPEC.md:
     *      {
     *        "totalBytes": 1441792,
     *        "usedBytes": 28672,
     *        "files": [
     *          {"name": "/file1.txt", "size": 120},
     *          {"name": "/file2.txt", "size": 85}
     *        ]
     *      }
     *    The loop below parses flat JSON and maps to array of objects item['name'], item['size'].
     */
    char *rawListJson = NULL;
    if (list_file("/", &rawListJson) == FS_OK && rawListJson != NULL) {
        JsonDocument listDoc;
        DeserializationError err = deserializeJson(listDoc, rawListJson);
        if (!err) {
            JsonObject rootObj = listDoc.as<JsonObject>();
            for (JsonPair kv : rootObj) {
                JsonObject item = filesArray.add<JsonObject>();
                String filename = kv.key().c_str();
                size_t size = kv.value().as<size_t>();
                item["name"] = filename;
                item["size"] = size;
            }
        }
        // Free heap buffer allocated by list_file() to prevent memory leaks
        free(rawListJson);
    }

    String responsePayload;
    serializeJson(doc, responsePayload);

    request->send(200, "application/json", responsePayload);
}

/** @brief Handles HTTP GET request for reading raw text content of a specific file ("/api/fs/read"). */
static void handleFSRead(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;

    if (!request->hasParam("path")) {
        request->send(400, "text/plain", "Missing 'path' parameter");
        return;
    }
    String path = normalize_fs_path(request->getParam("path")->value());

    char *fileData = NULL;
    unsigned int bytesRead = 0;
    FsResult_t result = read_file(path.c_str(), &fileData, &bytesRead);
    if (result != FS_OK) {
        send_fs_error(request, result);
        return;
    }

    String content(fileData);
    free(fileData);
    request->send(200, "text/plain", content);
}

/**
 * @brief Header callback for completing HTTP POST save/edit file ("/api/fs/save").
 * 
 * @details Executes after all chunks of POST body have been accumulated into UploadContext.
 *          Validates JSON payload (containing 'path' and 'content'), writes file to LittleFS,
 *          and safely frees memory to prevent Double Free.
 * 
 * @param[in] request Pointer to AsyncWebServerRequest object from client.
 */
static void handleFSSaveRequest(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    UploadContext *ctx = (UploadContext*)request->_tempObject; // Get accumulated context from handleFSSaveBody

    /*
     * MEMORY SAFETY & IDEMPOTENT CLEANUP (Anti-Double Free):
     * During body data upload, accumulate_body_chunk registered a callback
     * request->onDisconnect([]() { ... }). If client abruptly drops connection,
     * onDisconnect will check request->_tempObject and call delete if not yet freed.
     * 
     * To prevent DOUBLE FREE errors (Undefined Behavior / System Crash):
     * At any exit branch (allocation error, JSON parse error, or successful file write):
     * 1. Always execute `delete ctx;`
     * 2. IMMEDIATELY set `request->_tempObject = nullptr;`
     * Resetting the pointer to nullptr ensures idempotence: if the onDisconnect event
     * of AsyncWebServer continues to fire, it will see request->_tempObject == nullptr and ignore it.
     */
    // Check the integrity of the buffer and the received data size
    if (!ctx || !ctx->buffer || ctx->received_size != ctx->total_size) {
        request->send(500, "text/plain", "Upload failed or memory allocation error");
        if (ctx) {
            delete ctx;
            request->_tempObject = nullptr; // Reset pointer to prevent double free
        }
        return;
    }

  #ifdef SYSTEM_USES_PSRAM
    JsonDocument doc(&psramAlloc);
  #else
      JsonDocument doc;
  #endif
    DeserializationError err = deserializeJson(doc, (const char*)ctx->buffer, ctx->total_size);
    if (err || !doc.containsKey("path") || !doc.containsKey("content")) {
        delete ctx;
        request->_tempObject = nullptr; // Reset pointer before sending error response
        request->send(400, "text/plain", "Invalid JSON payload or missing path/content");
        return;
    }
    String path = normalize_fs_path(doc["path"].as<String>());
    const char *content = doc["content"];
    size_t write_length = strlen(content);

    unsigned int writtenBytes = 0;
    FsResult_t result = write_file(path.c_str(), content, write_length, &writtenBytes);

    // Immediately free payload buffer after writing to flash to return RAM to the system
    delete ctx;
    request->_tempObject = nullptr; // Ensure absolute safety if onDisconnect is called later

    if (result != FS_OK) {
        send_fs_error(request, result);
        return;
    }
    request->send(200, "text/plain", "OK");
}

/** @brief Handles HTTP POST body payload for creating or saving a LittleFS file ("/api/fs/save"). */
static void handleFSSaveBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!web_authenticate(request)) return;
    accumulate_body_chunk(request, data, len, index, total);
}

/** @brief Handles HTTP POST DELETE request */

/** @brief Handles HTTP POST request for renaming a file ("/api/fs/rename"). */
static void handleFSRenameRequest(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    if (!request->hasParam("path") || !request->hasParam("new_path")) {
        request->send(400, "text/plain", "Missing 'path' or 'new_path' parameter");
        return;
    }

    String path = normalize_fs_path(request->getParam("path")->value());
    String new_path = normalize_fs_path(request->getParam("new_path")->value());

    FsResult_t result = rename_file(path.c_str(), new_path.c_str());
    if (result != FS_OK) {
        send_fs_error(request, result);
        return;
    }
    request->send(200, "text/plain", "OK");
}

static void handleFSDeleteRequest(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    if (!request->hasParam("path")) {
        request->send(400, "text/plain", "Missing 'path' parameter");
        return;
    }

    String path = normalize_fs_path(request->getParam("path")->value());

    FsResult_t result = remove_file(path.c_str());
    if (result != FS_OK) {
        send_fs_error(request, result);
        return;
    }
    request->send(200, "text/plain", "OK");
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

void register_fs_routes(AsyncWebServer *server) {
    if (server == nullptr) return;

    server->on("/api/fs/list", HTTP_GET, handleFSList);
    server->on("/api/fs/read", HTTP_GET, handleFSRead);
    server->on("/api/fs/save", HTTP_POST, handleFSSaveRequest, NULL, handleFSSaveBody);
    server->on("/api/fs/delete", HTTP_POST, handleFSDeleteRequest);
}
