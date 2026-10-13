#include "services/web/web_routes_ota.h"
#include "services/web/web_auth.h"
#include "services/web/web_server_task.h"
#include "services/ota/ota_session.h"
#include "core/core_log.h"
#include "services/logger/log_task.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Update.h>

#define OTA_RESTART_DELAY_MS 500

/*
 * OTA ROUTES
 *
 * 1) CHUNKED API (used by the web page), the session logic lives in services/ota/ota_session.cpp:
 *
 *   GET  /api/ota/info                      capacity, chunk size, chip
 *   POST /api/ota/begin?size=&md5=          open a session
 *   POST /api/ota/chunk?session=&offset=&crc=   one chunk as raw body (octet-stream)
 *   POST /api/ota/end?session=              verify and reboot
 *   POST /api/ota/abort?session=            cancel
 *   GET  /api/ota/status                    progress / last error (resync after a dropped connection)
 *
 *   The chunk route has two callbacks, because the body arrives in small TCP pieces:
 *     handleChunkBody()     called for every received piece. Piece 0 validates the request (auth, session,
 *                           offset, length); every piece is copied into the chunk buffer; the last piece
 *                           validates the CRC and writes the chunk to flash.
 *     handleChunkResponse() called once after the whole body was received; it only answers with the result
 *                           that handleChunkBody() stored in the request (request->_tempObject).
 *
 * 2) LEGACY ROUTE POST /doUpdate (multipart, one stream), kept for curl:
 *
 *   Browser                           ESP32 (ESPAsyncWebServer)
 *   POST file ----------------------> handleOtaUpload()    called once per received piece
 *                                       piece 1 (index == 0): Update.begin()  -> open the next OTA partition
 *                                       every piece         : Update.write()  -> write it to flash
 *                                       last piece (final)  : Update.end()    -> verify image, set boot partition
 *                                                             and set s_otaFinished = true only if this succeeded
 *                                     handleOtaUpdateRequest() called ONCE after the whole body was received
 *   <---------------------------------  reads s_otaFinished: 200 "OK" (then reboot) or 500 "FAIL"
 */

/* -------------------------------------------------------------------------- */
/*                                    TYPES                                   */
/* -------------------------------------------------------------------------- */

/** Result of one chunk request, stored in request->_tempObject (allocated with calloc: the web server frees it). */
struct ChunkCtx {
    bool        authFailed;  /* Credentials were wrong: the response handler answers 401 */
    bool        failed;      /* The chunk was rejected: err holds the reason */
    bool        done;        /* The last body piece was processed */
    OtaError    err;         /* Failure description (valid when failed) */
    OtaProgress prog;        /* New progress (valid when done and not failed) */
};

/* -------------------------------------------------------------------------- */
/*                              STATIC VARIABLES                              */
/* -------------------------------------------------------------------------- */

/** Legacy route: true only when Update.end() succeeded for the upload currently being handled. */
static bool s_otaFinished = false;

/** Legacy route: true when the current upload was refused because a chunked session is open. */
static bool s_legacyRejected = false;

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Sends a JSON document as the response.
 * @param request Request to answer.
 * @param code HTTP status.
 * @param doc Document to serialize.
 */
static void sendJson(AsyncWebServerRequest *request, int code, const JsonDocument &doc) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    if (response == nullptr) {
        request->send(500, "text/plain", "Out of memory");
        return;
    }
    response->setCode(code);
    serializeJson(doc, *response);
    request->send(response);
}

/**
 * @brief Copies an OtaError into a JSON object using the API error shape.
 * @param obj Destination object.
 * @param e Error to describe.
 */
static void fillError(JsonObject obj, const OtaError &e) {
    obj["ok"] = false;
    obj["stage"] = e.stage;
    obj["code"] = e.code;
    obj["message"] = (const char *)e.message;
    if (e.chunk >= 0) obj["chunk"] = e.chunk;
    if (e.offset >= 0) obj["offset"] = (int64_t)e.offset;
    if (e.expectedOffset >= 0) obj["expected_offset"] = (int64_t)e.expectedOffset;
    obj["retryable"] = e.retryable;
}

/**
 * @brief Answers a request with an OTA error (status taken from the error).
 * @param request Request to answer.
 * @param e Error to send.
 */
static void sendOtaError(AsyncWebServerRequest *request, const OtaError &e) {
    JsonDocument doc;
    fillError(doc.to<JsonObject>(), e);
    sendJson(request, e.http, doc);
}

/**
 * @brief Rounds a percentage to one decimal for the JSON answers.
 * @param p Percentage.
 * @return Rounded value.
 */
static float roundPercent(float p) {
    return roundf(p * 10.0f) / 10.0f;
}

/**
 * @brief Reads a query parameter as text.
 * @param request Request.
 * @param name Parameter name.
 * @return The value, or nullptr when the parameter is missing.
 */
static const char *queryParam(AsyncWebServerRequest *request, const char *name) {
    if (!request->hasParam(name)) return nullptr;
    return request->getParam(name)->value().c_str();
}

/**
 * @brief Parses an unsigned number from text.
 * @param s Text (decimal or hexadecimal).
 * @param base 10 or 16.
 * @param out Receives the value.
 * @return true when the whole text is a valid number.
 */
static bool parseUnsigned(const char *s, int base, uint32_t &out) {
    if (s == nullptr || *s == '\0') return false;
    char *end = nullptr;
    unsigned long long v = strtoull(s, &end, base);
    if (end == s || *end != '\0' || v > 0xFFFFFFFFull) return false;
    out = (uint32_t)v;
    return true;
}

/**
 * @brief Schedules a reboot after the response has been sent.
 */
static void scheduleOtaRestart() {
    xTaskCreate([](void *arg) {
        vTaskDelay(pdMS_TO_TICKS(OTA_RESTART_DELAY_MS));
        postIncomingCommand(CMD_RESTART);
        vTaskDelete(NULL);
    }, "ota_restart_task", 2048, NULL, 1, NULL);
}

/**
 * @brief GET /api/ota/info: capacity and availability.
 * @param request Incoming request.
 */
static void handleOtaInfo(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;

    OtaInfo info;
    ota_get_info(info);

    JsonDocument doc;
    doc["busy"] = info.busy;
    doc["max_size"] = info.maxSize;
    doc["chunk_size"] = info.chunkSize;
    doc["chip"] = info.chip;
    doc["free_heap"] = info.freeHeap;
    sendJson(request, 200, doc);
}

/**
 * @brief POST /api/ota/begin: opens a session.
 * @param request Incoming request.
 */
static void handleOtaBegin(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;

    OtaError err;
    uint32_t size = 0;
    if (!parseUnsigned(queryParam(request, "size"), 10, size)) {
        ota_error_make(err, 400, "size", "bad_size", false, "Query parameter size is missing or invalid");
        sendOtaError(request, err);
        return;
    }

    char session[9] = {0};
    uint32_t chunkSize = 0;
    if (!ota_begin(size, queryParam(request, "md5"), session, chunkSize, err)) {
        sendOtaError(request, err);
        return;
    }

    JsonDocument doc;
    doc["ok"] = true;
    doc["session"] = (const char *)session;
    doc["chunk_size"] = chunkSize;
    doc["offset"] = 0;
    doc["total"] = size;
    sendJson(request, 200, doc);
}

/**
 * @brief POST /api/ota/chunk (body callback): validates on the first piece, collects every piece, writes on the last.
 * @param request Incoming request.
 * @param data Body piece.
 * @param len Piece length.
 * @param index Offset of this piece inside the body.
 * @param total Total body length.
 */
static void handleChunkBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    ChunkCtx *ctx = (ChunkCtx *)request->_tempObject;

    // Step 1: first piece -> create the result holder and validate the request before any data is accepted
    if (index == 0) {
        ctx = (ChunkCtx *)calloc(1, sizeof(ChunkCtx));
        if (ctx == nullptr) return;   // the response handler then answers with a generic failure
        request->_tempObject = ctx;

        // Authentication is only checked here (no response is sent from a body callback)
        if (!request->authenticate(getWebUsername().c_str(), getWebPassword().c_str())) {
            ctx->authFailed = true;
            return;
        }

        const char *session = queryParam(request, "session");
        uint32_t offset = 0, crc = 0;
        if (!parseUnsigned(queryParam(request, "offset"), 10, offset)) {
            ota_error_make(ctx->err, 400, "offset", "bad_offset", false, "Query parameter offset is missing or invalid");
            ctx->failed = true;
            return;
        }
        if (!parseUnsigned(queryParam(request, "crc"), 16, crc)) {
            ota_error_make(ctx->err, 400, "crc", "bad_crc", false, "Query parameter crc is missing or invalid");
            ctx->failed = true;
            return;
        }
        if (!ota_chunk_start(request, session, offset, crc, total, ctx->err)) {
            ctx->failed = true;
            return;
        }

        // If the client disconnects before the chunk is complete, release the chunk
        request->onDisconnect([request]() { ota_chunk_cancel(request); });
    }

    if (ctx == nullptr || ctx->authFailed || ctx->failed) return;

    // Step 2: copy this piece into the chunk buffer
    ota_chunk_append(request, data, len, index);

    // Step 3: last piece -> validate (length, CRC, image header) and write the whole chunk to flash
    if (index + len == total) {
        ctx->done = true;
        if (!ota_chunk_commit(request, ctx->prog, ctx->err)) {
            ctx->failed = true;
        }
    }
}

/**
 * @brief POST /api/ota/chunk (response callback): answers with the result stored by the body callback.
 * @param request Incoming request.
 */
static void handleChunkResponse(AsyncWebServerRequest *request) {
    ChunkCtx *ctx = (ChunkCtx *)request->_tempObject;

    // No context means the body never arrived (empty body) or memory ran out
    if (ctx == nullptr) {
        if (!web_authenticate(request)) return;
        OtaError err;
        ota_error_make(err, 400, "length", "empty_body", true, "Chunk body is empty or could not be read");
        sendOtaError(request, err);
        return;
    }
    if (ctx->authFailed) {
        web_authenticate(request);   // sends the 401 challenge
        return;
    }
    if (ctx->failed) {
        sendOtaError(request, ctx->err);
        return;
    }
    if (!ctx->done) {
        OtaError err;
        ota_error_make(err, 400, "length", "incomplete_body", true, "Chunk body incomplete");
        sendOtaError(request, err);
        return;
    }

    JsonDocument doc;
    doc["ok"] = true;
    doc["offset"] = ctx->prog.offset;
    doc["total"] = ctx->prog.total;
    doc["percent"] = roundPercent(ctx->prog.percent);
    sendJson(request, 200, doc);
}

/**
 * @brief POST /api/ota/end: verifies the image and reboots.
 * @param request Incoming request.
 */
static void handleOtaEnd(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;

    OtaError err;
    if (!ota_end(queryParam(request, "session"), err)) {
        sendOtaError(request, err);
        return;
    }

    JsonDocument doc;
    doc["ok"] = true;
    doc["reboot_in_ms"] = OTA_RESTART_DELAY_MS;
    sendJson(request, 200, doc);

    scheduleOtaRestart();
}

/**
 * @brief POST /api/ota/abort: cancels the open session.
 * @param request Incoming request.
 */
static void handleOtaAbort(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;

    OtaError err;
    if (!ota_abort(queryParam(request, "session"), err)) {
        sendOtaError(request, err);
        return;
    }

    JsonDocument doc;
    doc["ok"] = true;
    sendJson(request, 200, doc);
}

/**
 * @brief GET /api/ota/status: progress and last error.
 * @param request Incoming request.
 */
static void handleOtaStatus(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;

    OtaStatus st;
    ota_get_status(st);

    static const char *const names[] = { "idle", "receiving", "done", "failed" };

    JsonDocument doc;
    doc["state"] = names[st.state];
    doc["offset"] = st.offset;
    doc["total"] = st.total;
    doc["percent"] = roundPercent(st.percent);
    if (st.chunk >= 0) doc["chunk"] = st.chunk; else doc["chunk"] = nullptr;
    if (st.hasError) fillError(doc["error"].to<JsonObject>(), st.error); else doc["error"] = nullptr;
    sendJson(request, 200, doc);
}

/**
 * @brief Handles the final response for the legacy OTA firmware update ("/doUpdate").
 * @param request Incoming request; its whole body (the upload) has already been processed.
 */
static void handleOtaUpdateRequest(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;

    // Step 1: read and clear the result of the upload so it cannot leak into the next request.
    // "No update error" is not enough: an upload that never ran has no error either.
    bool success = s_otaFinished && !s_legacyRejected && !Update.hasError();
    s_otaFinished = false;
    s_legacyRejected = false;

    // Step 2: answer. A failure must not answer 2xx, otherwise clients treat it as a successful update.
    AsyncWebServerResponse *response = request->beginResponse(success ? 200 : 500, "text/plain", success ? "OK" : "FAIL");
    response->addHeader("Connection", "close");
    request->send(response);

    // Step 3: reboot into the new firmware, deferred so the response above can be sent first
    if (success) {
        scheduleOtaRestart();
    }
}

/**
 * @brief Handles streaming pieces of binary firmware data during the legacy OTA update ("/doUpdate").
 * @param request Incoming request.
 * @param filename Name of the uploaded file.
 * @param index Offset of this piece in the file (0 for the first piece).
 * @param data Piece data.
 * @param len Piece length in bytes.
 * @param final True for the last piece of the file.
 */
static void handleOtaUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!web_authenticate(request)) return;

    // Step 1: first piece -> start a new update and clear the result of any previous one
    if (!index) {
        s_otaFinished = false;
        s_legacyRejected = ota_is_busy();   // never fight a chunked session over the single Update object
        if (s_legacyRejected) {
            LOG_ERROR("OTA legacy upload refused: a chunked session is open");
        } else {
            LOG_INFO("OTA Update started, filename: " + filename);
            // UPDATE_SIZE_UNKNOWN: the size is not known up front, the whole next OTA partition is used
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                LOG_ERROR("OTA Begin Error!");
                Update.printError(Serial);
            }
        }
    }
    if (s_legacyRejected) return;

    // Step 2: write this piece to flash (a short write means an error is recorded in Update)
    if (Update.write(data, len) != len) {
        LOG_ERROR("OTA Write Error!");
        Update.printError(Serial);
    }

    // Step 3: last piece -> finish. end(true) accepts it even though the size was unknown; it validates
    // the image and switches the boot partition. Only a successful end() marks the OTA as finished.
    if (final) {
        if (Update.end(true)) {
            s_otaFinished = true;
            LOG_INFO("OTA Update Success! Written bytes: " + String(index + len));
        } else {
            LOG_ERROR("OTA End Error!");
            Update.printError(Serial);
        }
    }
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

void register_ota_routes(AsyncWebServer *server) {
    if (server == nullptr) return;

    // Chunked API
    server->on("/api/ota/info", HTTP_GET, handleOtaInfo);
    server->on("/api/ota/begin", HTTP_POST, handleOtaBegin);
    server->on("/api/ota/chunk", HTTP_POST, handleChunkResponse, nullptr, handleChunkBody);
    server->on("/api/ota/end", HTTP_POST, handleOtaEnd);
    server->on("/api/ota/abort", HTTP_POST, handleOtaAbort);
    server->on("/api/ota/status", HTTP_GET, handleOtaStatus);

    // Legacy single-stream route
    server->on("/doUpdate", HTTP_POST, handleOtaUpdateRequest, handleOtaUpload);
}

