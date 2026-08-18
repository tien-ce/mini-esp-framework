#include "core/web/web_routes_fs.h"
#include "core/web/web_auth.h"
#include "core/log_task.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief Handles HTTP GET request for LittleFS storage statistics and file directory list ("/api/fs/list"). */
static void handleFSList(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;

    if (!LittleFS.begin(true)) {
        request->send(500, "application/json", "{\"error\":\"LittleFS mount failed\"}");
        return;
    }

    JsonDocument doc;
    doc["totalBytes"] = LittleFS.totalBytes();
    doc["usedBytes"]  = LittleFS.usedBytes();
    JsonArray filesArray = doc["files"].to<JsonArray>();

    File root = LittleFS.open("/");
    if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while (file) {
            JsonObject item = filesArray.add<JsonObject>();
            String fname = file.name();
            if (!fname.startsWith("/")) {
                fname = "/" + fname;
            }
            item["name"] = fname;
            item["size"] = file.size();
            file = root.openNextFile();
        }
        root.close();
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
    // Body parsing handled in handleFSSaveBody
}

/** @brief Handles HTTP POST body payload for creating or saving a LittleFS file ("/api/fs/save"). */
static void handleFSSaveBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!web_authenticate(request)) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, (const char*)data, len);
    if (err || !doc.containsKey("path") || !doc.containsKey("content")) {
        request->send(400, "text/plain", "Invalid JSON payload or missing path/content");
        return;
    }

    String path = doc["path"].as<String>();
    String content = doc["content"].as<String>();
    if (!path.startsWith("/")) {
        path = "/" + path;
    }

    if (!LittleFS.begin(true)) {
        request->send(500, "text/plain", "LittleFS mount failed");
        return;
    }

    File f = LittleFS.open(path, "w");
    if (!f) {
        request->send(500, "text/plain", "Failed to open file for writing");
        return;
    }

    f.print(content);
    f.close();
    request->send(200, "text/plain", "OK");
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
