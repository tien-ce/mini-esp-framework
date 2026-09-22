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
 * @brief Xử lý HTTP GET request ("/api/fs/list") cung cấp thông số dung lượng và danh sách tập tin LittleFS.
 * 
 * @details Hàm thực hiện mapping cấu trúc dữ liệu theo hợp đồng giao tiếp API_FS_SPEC.md:
 *          - Truy vấn tổng dung lượng (totalBytes) và dung lượng đã dùng (usedBytes).
 *          - Gọi list_file("/") để lấy chuỗi JSON thô dạng flat dictionary {"filename": size, ...}.
 *          - Bóc tách và transform cấu trúc sang mảng đối tượng: files: [ { "name": ..., "size": ... }, ... ].
 *          - Tuân thủ hợp đồng sở hữu bộ nhớ: giải phóng bộ đệm rawListJson bằng free() ngay sau khi parse.
 * 
 * @param[in] request Con trỏ đối tượng AsyncWebServerRequest từ client.
 */
static void handleFSList(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;

    JsonDocument doc;
    doc["totalBytes"] = file_system_get_size();
    doc["usedBytes"]  = file_system_get_used();

    // Khởi tạo mảng "files": [] theo chuẩn API_FS_SPEC.md (Mục 2.1)
    JsonArray filesArray = doc["files"].to<JsonArray>();

    /*
     * AN TOÀN VÙNG NHỚ & MAPPING JSON THEO API_FS_SPEC.md:
     * 1. Hợp đồng sở hữu bộ nhớ: Hàm list_file() cấp phát động chuỗi rawListJson trên Heap.
     *    Caller có trách nhiệm bắt buộc phải giải phóng chuỗi này thông qua free() sau khi dùng.
     * 2. Cơ chế Transformation/Mapping:
     *    - Chuỗi trả về từ list_file() là dạng flat JSON: {"/file1.txt": 120, "/file2.txt": 85}
     *    - Giao diện Web Frontend yêu cầu format theo API_FS_SPEC.md:
     *      {
     *        "totalBytes": 1441792,
     *        "usedBytes": 28672,
     *        "files": [
     *          {"name": "/file1.txt", "size": 120},
     *          {"name": "/file2.txt", "size": 85}
     *        ]
     *      }
     *    Vòng lặp bên dưới parse flat JSON và map sang mảng đối tượng item["name"], item["size"].
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
        // Giải phóng heap buffer được cấp phát bởi list_file() để chống rò rỉ bộ nhớ
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
 * @brief Header callback hoàn tất xử lý HTTP POST lưu/chỉnh sửa file ("/api/fs/save").
 * 
 * @details Thực thi sau khi toàn bộ các chunks của POST body đã được tích lũy vào UploadContext.
 *          Hàm xác thực JSON payload (chứa "path" và "content"), ghi file xuống LittleFS,
 *          và thực hiện cơ chế giải phóng bộ nhớ an toàn chống Double Free.
 * 
 * @param[in] request Con trỏ đối tượng AsyncWebServerRequest từ client.
 */
static void handleFSSaveRequest(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    UploadContext *ctx = (UploadContext*)request->_tempObject; // Lấy context đã tích lũy trong handleFSSaveBody

    /*
     * CƠ CHẾ AN TOÀN VÙNG NHỚ & CHỐNG DOUBLE FREE (Memory Safety & Idempotent Cleanup):
     * Trong suốt quá trình upload dữ liệu body, hàm accumulate_body_chunk đã đăng ký một callback
     * request->onDisconnect([]() { ... }). Nếu client đột ngột ngắt kết nối mạng (abrupt drop),
     * onDisconnect sẽ kiểm tra request->_tempObject và gọi delete nếu chưa giải phóng.
     * 
     * Để tránh lỗi DOUBLE FREE (Undefined Behavior / Crash hệ thống):
     * Tại bất kỳ nhánh thoát nào (lỗi cấp phát, lỗi parse JSON, hoặc ghi file thành công):
     * 1. Luôn thực hiện `delete ctx;`
     * 2. NGAY LẬP TỨC gán `request->_tempObject = nullptr;`
     * Việc reset con trỏ về nullptr đảm bảo tính lũy đẳng (Idempotent): nếu sau đó sự kiện onDisconnect
     * của AsyncWebServer vẫn tiếp tục bắn ra, nó sẽ thấy request->_tempObject == nullptr và bỏ qua.
     */
    // Kiểm tra tính toàn vẹn của buffer và dung lượng dữ liệu nhận được
    if (!ctx || !ctx->buffer || ctx->received_size != ctx->total_size) {
        request->send(500, "text/plain", "Upload failed or memory allocation error");
        if (ctx) {
            delete ctx;
            request->_tempObject = nullptr; // Reset con trỏ để chống double free
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
        request->_tempObject = nullptr; // Reset con trỏ trước khi gửi response lỗi
        request->send(400, "text/plain", "Invalid JSON payload or missing path/content");
        return;
    }
    String path = normalize_fs_path(doc["path"].as<String>());
    const char *content = doc["content"];
    size_t write_length = strlen(content);

    unsigned int writtenBytes = 0;
    FsResult_t result = write_file(path.c_str(), content, write_length, &writtenBytes);

    // Giải phóng ngay lập tức buffer payload sau khi ghi flash xong để trả lại RAM cho hệ thống
    delete ctx;
    request->_tempObject = nullptr; // Đảm bảo an toàn tuyệt đối nếu onDisconnect được gọi sau đó

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
