#include "core/web/web_routes_fs.h"
#include "core/web/web_auth.h"
#include "core/log_task.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

static char *buffer = NULL;
static size_t buffer_length = 0;
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

/** @brief Handles HTTP GET request for LittleFS storage statistics and file directory list ("/api/fs/list"). */
/* 
{
  "totalBytes": 1441792,
  "usedBytes": 28672,
  "files": [
      {
      "name": "/web_config.txt",
      "size": 120
      }
  ]
}
*/
static void handleFSList(AsyncWebServerRequest *request) {
    if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
        return request->requestAuthentication();
    }

    JsonDocument doc;
    doc["totalBytes"] = file_system_get_size();
    doc["usedBytes"]  = file_system_get_used();

    // files: []
    JsonArray filesArray = doc["files"].to<JsonArray>();

    char *rawListJson = list_file("/");
    if (rawListJson != NULL) {
        JsonDocument listDoc;
        /* {
         * "name":
          "size":
           }
        */
        DeserializationError err = deserializeJson(listDoc, rawListJson);
        if (!err) {
            JsonObject rootObj = listDoc.as<JsonObject>();
            for (JsonPair kv : rootObj) {
                // Add an object{} to array []
                JsonObject item = filesArray.add<JsonObject>();
                // Store vale to object instance
                String filename = kv.key().c_str();
                size_t size = kv.value().as<size_t>();
                item["name"] = filename;
                item["size"] = size;
            }
        }
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

    String path = request->getParam("path")->value();

    if (!LittleFS.exists(path)) {
        request->send(404, "text/plain", "File not found");
        return;
    }

    File f = LittleFS.open(path, "r");
    if (!f) {
        request->send(500, "text/plain", "Failed to open file for reading");
        return;
    }

    String content = f.readString();
    f.close();
    request->send(200, "text/plain", content);
}

/** @brief Header callback placeholder for saving file content ("/api/fs/save"). */
static void handleFSSaveRequest(AsyncWebServerRequest *request) {
  #ifdef SYSTEM_USES_PSAM
    JsonDocument doc(&psramAlloc);
  #else
      JsonDocument doc;
  #endif
    DeserializationError err = deserializeJson(doc, (const char*)buffer, bytes_write);
    String path = doc["path"].as<String>();
    String content = doc["content"].as<String>();
    unsigned int write_length = content.length();
    if (err || !doc.containsKey("path") || !doc.containsKey("content")) {
        request->send(400, "text/plain", "Invalid JSON payload or missing path/content");
        return;
    }
    size_t write_length = write_file(path.c_str(),content.c_str(), bytes_write);
    /* Check if write full bytes */
    if(write_length != bytes_write)
    {
        request->send(500, "text/plain", "Write %d bytes instead of %d bytes", bytes_write, write_length);
        return;
    }
    request->send(200, "text/plain", "OK");
    free(buffer);
}

/** @brief Handles HTTP POST body payload for creating or saving a LittleFS file ("/api/fs/save"). */
static void handleFSSaveBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!web_authenticate(request)) return;
    // Allocate more buffe
    size_t new_lenght = buffer_length + len; 
    #ifdef SYSTEM_USES_PSAM
      buffer = ps_realloc(buffer, sizeof(char) * (new_lenght));
    #else
      buffer = realloc(buffer, sizeof(char) * (new_lenght));
    #endif
    // Copy data to buffer
    memcpy(buffer[buffer_length], data, len);
}

/** @brief Header callback placeholder for deleting a file ("/api/fs/delete"). */
static void handleFSDeleteRequest(AsyncWebServerRequest *request) {
    // Body parsing handled in handleFSDeleteBody
}

/** @brief Handles HTTP POST body payload for deleting a LittleFS file ("/api/fs/delete"). */
static void handleFSDeleteBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!web_authenticate(request)) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, (const char*)data, len);
    if (err || !doc.containsKey("path")) {
        request->send(400, "text/plain", "Invalid JSON payload or missing path");
        return;
    }

    String path = doc["path"].as<String>();
    if (!path.startsWith("/")) {
        path = "/" + path;
    }

    if (!LittleFS.begin(true)) {
        request->send(500, "text/plain", "LittleFS mount failed");
        return;
    }

    if (!LittleFS.exists(path)) {
        request->send(404, "text/plain", "File not found");
        return;
    }

    if (LittleFS.remove(path)) {
        request->send(200, "text/plain", "OK");
    } else {
        request->send(500, "text/plain", "Failed to delete file");
    }
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

void register_fs_routes(AsyncWebServer *server) {
    if (server == nullptr) return;

    server->on("/api/fs/list", HTTP_GET, handleFSList);
    server->on("/api/fs/read", HTTP_GET, handleFSRead);
    server->on("/api/fs/save", HTTP_POST, handleFSSaveRequest, NULL, handleFSSaveBody);
    server->on("/api/fs/delete", HTTP_POST, handleFSDeleteRequest, NULL, handleFSDeleteBody);
}
