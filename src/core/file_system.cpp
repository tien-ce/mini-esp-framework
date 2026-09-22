#include "core/file_system.h"
#include "built_in.h"
#include "TienInterpreter.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Tracks whether LittleFS.begin() succeeded, so every access function below
// can fail fast with FS_ERR_NOT_MOUNTED instead of letting LittleFS surface
// its own generic "not mounted" error deep inside open()/exists().
static bool g_fs_mounted = false;


const char *file_system_strerror(FsResult_t result)
{
    switch (result) {
        case FS_OK:                   return "OK";
        case FS_ERR_NOT_MOUNTED:      return "File system is not mounted";
        case FS_ERR_NOT_FOUND:        return "File or directory not found";
        case FS_ERR_IS_DIRECTORY:     return "Path is a directory, not a file";
        case FS_ERR_NOT_A_DIRECTORY:  return "Path is a file, not a directory";
        case FS_ERR_ALREADY_EXISTS:   return "Path already exists";
        case FS_ERR_OPEN_FAILED:      return "Failed to open path";
        case FS_ERR_ALLOC_FAILED:     return "Memory allocation failed";
        case FS_ERR_WRITE_INCOMPLETE: return "Not all bytes were written";
        case FS_ERR_OPERATION_FAILED: return "File system operation failed";
        case FS_ERR_INVALID_ARG:      return "Invalid argument";
        default:                      return "Unknown file system error";
    }
}

bool file_system_init(bool formatonfail, const char *basepath, uint8_t maxopenfiles)
{
    g_fs_mounted = LittleFS.begin(formatonfail, basepath, maxopenfiles);
    return g_fs_mounted;
}

bool file_system_format()
{
    return LittleFS.format();
}

size_t file_system_get_size()
{
    return LittleFS.totalBytes();
}
size_t file_system_get_used()
{
    return LittleFS.usedBytes();
}

/* file and directory interaction */

/*
 * Luồng thực thi của read_file():
 * 1. Khởi tạo giá trị mặc định cho các out-pointer (*out_data = NULL, *out_bytes_read = 0).
 * 2. Kiểm tra tính hợp lệ của con trỏ đầu vào (fail-fast với FS_ERR_INVALID_ARG).
 * 3. Kiểm tra trạng thái mount của LittleFS (FS_ERR_NOT_MOUNTED).
 * 4. Mở file theo cơ chế RAII: LittleFS File tự động đóng tài nguyên khi ra khỏi phạm vi (destructor).
 * 5. Phân loại mã lỗi khi mở file thất bại (FS_ERR_NOT_FOUND nếu không tồn tại, FS_ERR_OPEN_FAILED nếu lỗi hệ thống).
 * 6. Kiểm tra an toàn: từ chối nếu đường dẫn trỏ tới một thư mục (FS_ERR_IS_DIRECTORY).
 * 7. Cấp phát bộ nhớ động (ưu tiên PSRAM nếu có macro SYSTEM_USES_PSRAM, ngược lại dùng Internal Heap qua malloc).
 *    Kích thước cấp phát: size + 1 byte để đảm bảo chứa ký tự kết thúc chuỗi '\0'.
 * 8. Đọc dữ liệu từ flash vào buffer, gán ký tự null-terminator.
 * 9. Chuyển giao quyền sở hữu vùng nhớ (Ownership Transfer): Gán con trỏ buffer sang *out_data.
 *    LƯU Ý: Caller có toàn quyền và chịu trách nhiệm gọi free(*out_data) sau khi sử dụng để tránh memory leak.
 */
FsResult_t read_file(const char *path, char **out_data, unsigned int *out_bytes_read)
{
    if (out_data) *out_data = NULL;
    if (out_bytes_read) *out_bytes_read = 0;
    if (!path || !out_data || !out_bytes_read) return FS_ERR_INVALID_ARG;
    if (!g_fs_mounted) return FS_ERR_NOT_MOUNTED;

    // `file` closes itself in its own destructor (VFSFileImpl::~VFSFileImpl),
    // so every early return below just lets it go out of scope instead of
    // having to remember an explicit file.close() on each branch.
    File file = LittleFS.open(path, "r", false);
    if (!file) {
        // open() doesn't say *why* it failed - only pay for exists() to disambiguate on failure.
        return LittleFS.exists(path) ? FS_ERR_OPEN_FAILED : FS_ERR_NOT_FOUND;
    }
    if (file.isDirectory()) {
        return FS_ERR_IS_DIRECTORY;
    }

    size_t size = file.size();
    // Cấp phát vùng nhớ đệm động (Dynamic Buffer Allocation):
    // Caller kế thừa quyền sở hữu vùng nhớ này và bắt buộc phải giải phóng bằng free().
    #ifdef SYSTEM_USES_PSRAM
    char *buffer = (char*) ps_malloc(size + 1);
    #else
    char *buffer = (char*)malloc(size + 1);
    #endif
    if (!buffer) {
        return FS_ERR_ALLOC_FAILED;
    }

    size_t readLen = file.readBytes(buffer, size);
    buffer[readLen] = '\0';

    // Chuyển giao quyền sở hữu bộ nhớ cho caller
    *out_data = buffer;
    *out_bytes_read = readLen;
    return FS_OK;
}

FsResult_t write_file(const char *path, const char *data, unsigned int length, unsigned int *out_bytes_written)
{
    if (out_bytes_written) *out_bytes_written = 0;
    if (!path || !data) return FS_ERR_INVALID_ARG;
    if (!g_fs_mounted) return FS_ERR_NOT_MOUNTED;

    // `file` closes itself in its own destructor - no explicit close() needed on any branch.
    File file = LittleFS.open(path, "w", false);
    if (!file) return FS_ERR_OPEN_FAILED;
    if (file.isDirectory()) {
        return FS_ERR_IS_DIRECTORY;
    }

    size_t bytesWrite = file.write((const uint8_t*)data, length);

    if (out_bytes_written) *out_bytes_written = (unsigned int)bytesWrite;
    return (bytesWrite == length) ? FS_OK : FS_ERR_WRITE_INCOMPLETE;
}

/*
 * Luồng thực thi của list_file():
 * 1. Khởi tạo *out_json = NULL; kiểm tra tham số đầu vào và trạng thái mount của LittleFS.
 * 2. Mở thư mục dir_path theo cơ chế RAII; xác thực đây là một thư mục hợp lệ (!root.isDirectory() -> FS_ERR_NOT_A_DIRECTORY).
 * 3. Duyệt tuần tự các entry con thông qua root.openNextFile(), bỏ qua các thư mục con và thu thập cặp key-value
 *    (file.name(): file.size()) vào cấu trúc JsonDocument của ArduinoJson.
 * 4. Đo đạc kích thước chuỗi JSON chính xác qua measureJson(doc) + 1 byte cho ký tự '\0'.
 * 5. Cấp phát vùng nhớ động trên Heap thông qua malloc() cho chuỗi kết quả.
 * 6. Serialize nội dung JSON vào jsonBuffer và chuyển giao quyền sở hữu vùng nhớ cho caller (*out_json = jsonBuffer).
 *    LƯU Ý: Caller chịu trách nhiệm gọi free(*out_json) sau khi hoàn tất sử dụng.
 */
FsResult_t list_file(const char *dir_path, char **out_json)
{
    if (out_json) *out_json = NULL;
    if (!dir_path || !out_json) return FS_ERR_INVALID_ARG;
    if (!g_fs_mounted) return FS_ERR_NOT_MOUNTED;

    // `root` (and each `file`) closes itself in its own destructor - no explicit close() needed.
    File root = LittleFS.open(dir_path);
    if (!root) {
        return LittleFS.exists(dir_path) ? FS_ERR_OPEN_FAILED : FS_ERR_NOT_FOUND;
    }
    if (!root.isDirectory()) {
        return FS_ERR_NOT_A_DIRECTORY;
    }

    // Thu thập danh sách tập tin và kích thước vào JsonDocument
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            obj[file.name()] = file.size();
        }
        file = root.openNextFile();
    }

    // Đo đạc kích thước và cấp phát động chuỗi JSON trên Heap
    size_t jsonLen = measureJson(doc) + 1;
    char *jsonBuffer = (char*)malloc(jsonLen);
    if (!jsonBuffer) {
        return FS_ERR_ALLOC_FAILED;
    }
    serializeJson(doc, jsonBuffer, jsonLen);

    // Chuyển giao quyền sở hữu con trỏ bộ nhớ cho caller (caller phải gọi free())
    *out_json = jsonBuffer;
    return FS_OK;
}

FsResult_t remove_file(const char *path)
{
    if (!path) return FS_ERR_INVALID_ARG;
    if (!g_fs_mounted) return FS_ERR_NOT_MOUNTED;
    if (!LittleFS.exists(path)) return FS_ERR_NOT_FOUND;
    return LittleFS.remove(path) ? FS_OK : FS_ERR_OPERATION_FAILED;
}

FsResult_t rename_file(const char *pathFrom, const char *pathTo)
{
    if (!pathFrom || !pathTo) return FS_ERR_INVALID_ARG;
    if (!g_fs_mounted) return FS_ERR_NOT_MOUNTED;
    if (!LittleFS.exists(pathFrom)) return FS_ERR_NOT_FOUND;
    return LittleFS.rename(pathFrom, pathTo) ? FS_OK : FS_ERR_OPERATION_FAILED;
}

FsResult_t make_directory(const char *path)
{
    if (!path) return FS_ERR_INVALID_ARG;
    if (!g_fs_mounted) return FS_ERR_NOT_MOUNTED;
    if (LittleFS.exists(path)) return FS_ERR_ALREADY_EXISTS;
    return LittleFS.mkdir(path) ? FS_OK : FS_ERR_OPERATION_FAILED;
}

FsResult_t remove_dir(const char *path)
{
    if (!path) return FS_ERR_INVALID_ARG;
    if (!g_fs_mounted) return FS_ERR_NOT_MOUNTED;
    if (!LittleFS.exists(path)) return FS_ERR_NOT_FOUND;
    return LittleFS.rmdir(path) ? FS_OK : FS_ERR_OPERATION_FAILED;
}
