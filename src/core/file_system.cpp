#include "core/file_system.h"
#include "built_in.h"
#include "TienInterpreter.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <unordered_map>
#include <string>

// Tracks whether LittleFS.begin() succeeded, so every access function below
// can fail fast with FS_ERR_NOT_MOUNTED instead of letting LittleFS surface
// its own generic "not mounted" error deep inside open()/exists().
static bool g_fs_mounted = false;

/* ------------------------------------------------------------------------
 * Dentry (Directory Entry) cache
 *
 * LittleFS keeps no in-RAM directory/inode cache of its own: every
 * exists()/open()/isDirectory() call re-walks the on-flash metadata
 * structures, which is slow relative to a RAM hash lookup and adds needless
 * flash reads. This cache mirrors, for a bounded set of recently-touched
 * paths, the three facts every caller in this file actually needs: does the
 * path exist, is it a file or a directory, and (for files) how big is it.
 *
 * It is a pure lookaside cache: LittleFS is always the source of truth.
 * Entries are only ever populated from a real LittleFS answer, and every
 * mutating operation (write/remove/rename/mkdir/rmdir/format) invalidates
 * exactly the entries it can make stale, so a cache hit can never outlive
 * the flash state it describes.
 * ------------------------------------------------------------------------ */

namespace {

/** @brief Maximum number of paths the in-memory dentry cache may hold. Bounded to keep RAM use predictable on constrained ESP32/ESP32-S3 targets. */
constexpr size_t DENTRY_CACHE_MAX_ENTRIES = 48;

/**
 * @brief Cached metadata for a single normalized path.
 * @details Mirrors just enough of LittleFS's answer for a path to short-circuit
 *          the exists()/open()/isDirectory() calls that every public function
 *          in this file would otherwise repeat against flash.
 */
struct DentryEntry {
    bool exists;         /**< false = negative cache entry (path is known NOT to exist) */
    bool is_directory;   /**< Meaningful only when exists == true */
    size_t size;         /**< File size in bytes; 0 for directories or negative entries */
    uint32_t last_used;  /**< Logical clock tick of last access, used for LRU eviction */
};

std::unordered_map<std::string, DentryEntry> g_dentry_cache;
SemaphoreHandle_t g_dentry_mutex = nullptr;
uint32_t g_dentry_clock = 0;

/**
 * @brief RAII guard that takes the dentry cache mutex for the current scope.
 * @details Degrades gracefully to a no-op if the mutex failed to allocate
 *          (see dentry_cache_reset()), so a low-memory condition disables the
 *          cache instead of taking down the file system layer.
 */
class DentryLock {
public:
    DentryLock() {
        if (g_dentry_mutex) xSemaphoreTake(g_dentry_mutex, portMAX_DELAY);
    }
    ~DentryLock() {
        if (g_dentry_mutex) xSemaphoreGive(g_dentry_mutex);
    }
    DentryLock(const DentryLock &) = delete;
    DentryLock &operator=(const DentryLock &) = delete;
};

/**
 * @brief Normalizes a caller-supplied path into the canonical form used as a cache key.
 * @param path Raw path as received from a public API call (must be non-NULL).
 * @param out  Receives the normalized path: always starts with '/', and never
 *             ends with '/' unless it IS the root path "/". This ensures
 *             "/dir" and "/dir/" hash to the same cache entry.
 * @return void
 */
void dentry_normalize(const char *path, std::string &out)
{
    out = path;
    if (out.empty() || out.front() != '/') {
        out.insert(out.begin(), '/');
    }
    while (out.size() > 1 && out.back() == '/') {
        out.pop_back();
    }
}

/**
 * @brief Evicts the least-recently-used entry from the cache.
 * @details Assumes g_dentry_mutex is already held by the caller. Uses a linear
 *          scan, which is acceptable because DENTRY_CACHE_MAX_ENTRIES is small
 *          and this only runs when the cache is full and a genuinely new key
 *          is about to be inserted.
 * @return void
 */
void dentry_evict_lru_locked()
{
    if (g_dentry_cache.empty()) return;
    auto oldest = g_dentry_cache.begin();
    for (auto it = g_dentry_cache.begin(); it != g_dentry_cache.end(); ++it) {
        if (it->second.last_used < oldest->second.last_used) {
            oldest = it;
        }
    }
    g_dentry_cache.erase(oldest);
}

/**
 * @brief Looks up a normalized path in the dentry cache.
 * @param normalized_path Path already run through dentry_normalize().
 * @param[out] out Filled with the cached entry (positive or negative) when found.
 * @return true if a cached entry was found, false on a cold miss (caller must ask LittleFS).
 */
bool dentry_cache_lookup(const std::string &normalized_path, DentryEntry &out)
{
    DentryLock lock;
    auto it = g_dentry_cache.find(normalized_path);
    if (it == g_dentry_cache.end()) return false;
    it->second.last_used = ++g_dentry_clock;
    out = it->second;
    return true;
}

/**
 * @brief Inserts or refreshes a cache entry for a normalized path.
 * @param normalized_path Path already run through dentry_normalize().
 * @param exists       Whether LittleFS confirmed this path exists.
 * @param is_directory Whether the path is a directory (ignored when exists == false).
 * @param size         File size in bytes (ignored for directories / negative entries).
 * @return void
 */
void dentry_cache_put(const std::string &normalized_path, bool exists, bool is_directory, size_t size)
{
    DentryLock lock;
    if (g_dentry_cache.find(normalized_path) == g_dentry_cache.end() &&
        g_dentry_cache.size() >= DENTRY_CACHE_MAX_ENTRIES) {
        dentry_evict_lru_locked();
    }
    DentryEntry &entry = g_dentry_cache[normalized_path];
    entry.exists = exists;
    entry.is_directory = exists && is_directory;
    entry.size = (exists && !is_directory) ? size : 0;
    entry.last_used = ++g_dentry_clock;
}

/**
 * @brief Removes a normalized path AND every cached entry nested under it.
 * @details Required after rename/rmdir: a directory mutation can move or
 *          remove an entire subtree in one LittleFS call, and every path
 *          string the cache holds below that directory (e.g. "/dir/child.txt"
 *          under "/dir") becomes stale in that same instant. The cache has
 *          no parent/child links to patch incrementally - only full path
 *          strings - so the whole subtree is dropped and lazily re-populated
 *          from LittleFS on next access.
 * @param normalized_path Path already run through dentry_normalize().
 * @return void
 */
void dentry_cache_invalidate_subtree(const std::string &normalized_path)
{
    DentryLock lock;
    const std::string prefix = (normalized_path == "/") ? "/" : normalized_path + "/";
    for (auto it = g_dentry_cache.begin(); it != g_dentry_cache.end(); ) {
        if (it->first == normalized_path || it->first.compare(0, prefix.size(), prefix) == 0) {
            it = g_dentry_cache.erase(it);
        } else {
            ++it;
        }
    }
}

/**
 * @brief Discards every cached entry and lazily (re)creates the guarding mutex.
 * @details Called on mount, re-mount and format, where the entire on-flash
 *          state the cache was mirroring may no longer correspond to reality.
 * @return void
 */
void dentry_cache_reset()
{
    if (!g_dentry_mutex) {
        // Created once, on first use. If allocation fails (heap exhaustion),
        // g_dentry_mutex stays NULL: DentryLock then silently skips locking
        // and every lookup reports a cold miss, so the cache degrades to a
        // permanent no-op instead of taking down the file system layer.
        g_dentry_mutex = xSemaphoreCreateMutex();
    }
    DentryLock lock;
    g_dentry_cache.clear();
    g_dentry_clock = 0;
}

} // namespace

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

/** @brief Mounts LittleFS and resets the dentry cache so it starts clean for the new mount. */
bool file_system_init(bool formatonfail, const char *basepath, uint8_t maxopenfiles)
{
    g_fs_mounted = LittleFS.begin(formatonfail, basepath, maxopenfiles);

    // Whatever was cached before this call (from a previous mount, or none)
    // no longer applies to the filesystem we just (tried to) mount. Reset
    // unconditionally - on success AND on failure - so a failed re-init can
    // never leave stale hits behind for a later successful one.
    dentry_cache_reset();

    return g_fs_mounted;
}

/** @brief Formats LittleFS and invalidates the entire dentry cache, since every path it described is destroyed. */
bool file_system_format()
{
    bool ok = LittleFS.format();
    if (ok) {
        // A format wipes every file and directory on flash; every cached
        // dentry - positive or negative - is now meaningless.
        dentry_cache_reset();
    }
    return ok;
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
 * 4. Tra cứu dentry cache: nếu đã biết path không tồn tại hoặc là thư mục,
 *    trả về ngay mà không chạm vào flash.
 * 5. Mở file theo cơ chế RAII: LittleFS File tự động đóng tài nguyên khi ra khỏi phạm vi (destructor).
 * 6. Phân loại mã lỗi khi mở file thất bại (FS_ERR_NOT_FOUND nếu không tồn tại, FS_ERR_OPEN_FAILED nếu lỗi hệ thống).
 * 7. Kiểm tra an toàn: từ chối nếu đường dẫn trỏ tới một thư mục (FS_ERR_IS_DIRECTORY).
 * 8. Cấp phát bộ nhớ động (ưu tiên PSRAM nếu có macro SYSTEM_USES_PSRAM, ngược lại dùng Internal Heap qua malloc).
 *    Kích thước cấp phát: size + 1 byte để đảm bảo chứa ký tự kết thúc chuỗi '\0'.
 * 9. Đọc dữ liệu từ flash vào buffer, gán ký tự null-terminator.
 * 10. Cập nhật dentry cache với kết quả (tồn tại, không phải thư mục, kích thước).
 * 11. Chuyển giao quyền sở hữu vùng nhớ (Ownership Transfer): Gán con trỏ buffer sang *out_data.
 *     LƯU Ý: Caller có toàn quyền và chịu trách nhiệm gọi free(*out_data) sau khi sử dụng để tránh memory leak.
 */
FsResult_t read_file(const char *path, char **out_data, unsigned int *out_bytes_read)
{
    if (out_data) *out_data = NULL;
    if (out_bytes_read) *out_bytes_read = 0;
    if (!path || !out_data || !out_bytes_read) return FS_ERR_INVALID_ARG;
    if (!g_fs_mounted) return FS_ERR_NOT_MOUNTED;

    std::string key;
    dentry_normalize(path, key);

    // Dentry cache fast path: a prior operation already told us this path
    // does not exist, or is a directory - both are terminal answers for a
    // read, so return immediately instead of paying for a flash access whose
    // outcome we already know.
    DentryEntry cached;
    if (dentry_cache_lookup(key, cached)) {
        if (!cached.exists) return FS_ERR_NOT_FOUND;
        if (cached.is_directory) return FS_ERR_IS_DIRECTORY;
    }

    // `file` closes itself in its own destructor (VFSFileImpl::~VFSFileImpl),
    // so every early return below just lets it go out of scope instead of
    // having to remember an explicit file.close() on each branch.
    File file = LittleFS.open(path, "r", false);
    if (!file) {
        // open() doesn't say *why* it failed - only pay for exists() to disambiguate on failure.
        bool found = LittleFS.exists(path);
        if (!found) {
            dentry_cache_put(key, false, false, 0);
            return FS_ERR_NOT_FOUND;
        }
        // Exists but couldn't be opened (e.g. corrupt entry, too many open
        // files): an ambiguous, transient state that isn't safe to cache.
        return FS_ERR_OPEN_FAILED;
    }
    if (file.isDirectory()) {
        dentry_cache_put(key, true, true, 0);
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
    dentry_cache_put(key, true, false, readLen);
    return FS_OK;
}

/** @brief Writes (creating or truncating) a file, using the dentry cache to reject a known directory path without touching flash. */
FsResult_t write_file(const char *path, const char *data, unsigned int length, unsigned int *out_bytes_written)
{
    if (out_bytes_written) *out_bytes_written = 0;
    if (!path || !data) return FS_ERR_INVALID_ARG;
    if (!g_fs_mounted) return FS_ERR_NOT_MOUNTED;

    std::string key;
    dentry_normalize(path, key);

    // Fast path: a directory can never be opened for write. If the cache
    // already knows this path is a directory, reject it before touching flash.
    DentryEntry cached;
    if (dentry_cache_lookup(key, cached) && cached.exists && cached.is_directory) {
        return FS_ERR_IS_DIRECTORY;
    }

    // `file` closes itself in its own destructor - no explicit close() needed on any branch.
    File file = LittleFS.open(path, "w", false);
    if (!file) return FS_ERR_OPEN_FAILED;
    if (file.isDirectory()) {
        dentry_cache_put(key, true, true, 0);
        return FS_ERR_IS_DIRECTORY;
    }

    size_t bytesWrite = file.write((const uint8_t*)data, length);

    if (out_bytes_written) *out_bytes_written = (unsigned int)bytesWrite;

    // "w" mode truncates the file before writing, so bytesWrite is the file's
    // real size on flash now, whether or not the write was complete.
    dentry_cache_put(key, true, false, bytesWrite);

    return (bytesWrite == length) ? FS_OK : FS_ERR_WRITE_INCOMPLETE;
}

/*
 * Luồng thực thi của list_file():
 * 1. Khởi tạo *out_json = NULL; kiểm tra tham số đầu vào và trạng thái mount của LittleFS.
 * 2. Tra cứu dentry cache cho dir_path: nếu đã biết không tồn tại hoặc không phải thư mục,
 *    trả về ngay mà không mở flash.
 * 3. Mở thư mục dir_path theo cơ chế RAII; xác thực đây là một thư mục hợp lệ (!root.isDirectory() -> FS_ERR_NOT_A_DIRECTORY).
 * 4. Duyệt tuần tự các entry con thông qua root.openNextFile(), bỏ qua các thư mục con và thu thập cặp key-value
 *    (file.name(): file.size()) vào cấu trúc JsonDocument của ArduinoJson. Đồng thời "làm nóng" (warm up)
 *    dentry cache cho từng entry con, vì openNextFile() đã trả về tên và kích thước miễn phí.
 * 5. Đo đạc kích thước chuỗi JSON chính xác qua measureJson(doc) + 1 byte cho ký tự '\0'.
 * 6. Cấp phát vùng nhớ động trên Heap thông qua malloc() cho chuỗi kết quả.
 * 7. Serialize nội dung JSON vào jsonBuffer và chuyển giao quyền sở hữu vùng nhớ cho caller (*out_json = jsonBuffer).
 *    LƯU Ý: Caller chịu trách nhiệm gọi free(*out_json) sau khi hoàn tất sử dụng.
 */
FsResult_t list_file(const char *dir_path, char **out_json)
{
    if (out_json) *out_json = NULL;
    if (!dir_path || !out_json) return FS_ERR_INVALID_ARG;
    if (!g_fs_mounted) return FS_ERR_NOT_MOUNTED;

    std::string key;
    dentry_normalize(dir_path, key);

    // Fast path: skip the open() entirely when the cache already knows this
    // path doesn't exist, or isn't a directory.
    DentryEntry cached;
    if (dentry_cache_lookup(key, cached)) {
        if (!cached.exists) return FS_ERR_NOT_FOUND;
        if (!cached.is_directory) return FS_ERR_NOT_A_DIRECTORY;
    }

    // `root` (and each `file`) closes itself in its own destructor - no explicit close() needed.
    File root = LittleFS.open(dir_path);
    if (!root) {
        bool found = LittleFS.exists(dir_path);
        if (!found) dentry_cache_put(key, false, false, 0);
        return found ? FS_ERR_OPEN_FAILED : FS_ERR_NOT_FOUND;
    }
    if (!root.isDirectory()) {
        dentry_cache_put(key, true, false, root.size());
        return FS_ERR_NOT_A_DIRECTORY;
    }
    dentry_cache_put(key, true, true, 0);

    // Thu thập danh sách tập tin và kích thước vào JsonDocument
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            obj[file.name()] = file.size();

            // Opportunistic cache warm-up: openNextFile() already paid the
            // flash-read cost to learn this child's name and size, so cache
            // it now - a later read_file()/write_file()/remove_file() on the
            // same child gets a free cache hit instead of a second flash
            // lookup. file.name() is defensively treated as either a bare
            // leaf name or an already-absolute path, since the exact form
            // returned here varies across LittleFS/Arduino core versions.
            const char *name = file.name();
            if (name && name[0] != '\0') {
                std::string child_key = (name[0] == '/')
                    ? std::string(name)
                    : (key == "/" ? ("/" + std::string(name)) : (key + "/" + std::string(name)));
                dentry_cache_put(child_key, true, false, file.size());
            }
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

/** @brief Removes a file, using the dentry cache to fail fast on a known-absent path and negative-caching the path once removed. */
FsResult_t remove_file(const char *path)
{
    if (!path) return FS_ERR_INVALID_ARG;
    if (!g_fs_mounted) return FS_ERR_NOT_MOUNTED;

    std::string key;
    dentry_normalize(path, key);

    DentryEntry cached;
    if (dentry_cache_lookup(key, cached) && !cached.exists) {
        return FS_ERR_NOT_FOUND;
    }
    if (!LittleFS.exists(path)) {
        dentry_cache_put(key, false, false, 0);
        return FS_ERR_NOT_FOUND;
    }

    if (!LittleFS.remove(path)) return FS_ERR_OPERATION_FAILED;

    // The path is gone: record a negative entry immediately so a repeated
    // access (e.g. a client retrying against a stale link) fails fast
    // instead of hitting flash again.
    dentry_cache_put(key, false, false, 0);
    return FS_OK;
}

/** @brief Renames a path, invalidating the cached subtree at both the source and destination since a rename can move an entire directory at once. */
FsResult_t rename_file(const char *pathFrom, const char *pathTo)
{
    if (!pathFrom || !pathTo) return FS_ERR_INVALID_ARG;
    if (!g_fs_mounted) return FS_ERR_NOT_MOUNTED;

    std::string fromKey;
    dentry_normalize(pathFrom, fromKey);

    DentryEntry cached;
    if (dentry_cache_lookup(fromKey, cached) && !cached.exists) {
        return FS_ERR_NOT_FOUND;
    }
    if (!LittleFS.exists(pathFrom)) {
        dentry_cache_put(fromKey, false, false, 0);
        return FS_ERR_NOT_FOUND;
    }

    if (!LittleFS.rename(pathFrom, pathTo)) return FS_ERR_OPERATION_FAILED;

    // A rename can move an entire directory subtree in one call; every path
    // string the cache holds for pathFrom (and, if pathTo already existed,
    // whatever it used to point at) is now wrong. The cache has no
    // parent/child links to patch in place, so invalidate both subtrees
    // wholesale and let the next access on each re-populate it from LittleFS.
    std::string toKey;
    dentry_normalize(pathTo, toKey);
    dentry_cache_invalidate_subtree(fromKey);
    dentry_cache_invalidate_subtree(toKey);

    return FS_OK;
}

/** @brief Creates a directory, using the dentry cache to short-circuit an already-exists check and populating the cache on success. */
FsResult_t make_directory(const char *path)
{
    if (!path) return FS_ERR_INVALID_ARG;
    if (!g_fs_mounted) return FS_ERR_NOT_MOUNTED;

    std::string key;
    dentry_normalize(path, key);

    DentryEntry cached;
    bool cache_hit = dentry_cache_lookup(key, cached);
    if (cache_hit && cached.exists) {
        return FS_ERR_ALREADY_EXISTS;
    }
    // On a negative cache hit we trust the cache and skip the exists() call;
    // on a cold miss we still have to ask LittleFS, exactly as before caching
    // existed. Worst case if the cache were ever wrong here is a slightly
    // less specific FS_ERR_OPERATION_FAILED from mkdir() instead of
    // FS_ERR_ALREADY_EXISTS - never data loss or a false success.
    if (!cache_hit && LittleFS.exists(path)) {
        dentry_cache_put(key, true, true, 0);
        return FS_ERR_ALREADY_EXISTS;
    }

    if (!LittleFS.mkdir(path)) return FS_ERR_OPERATION_FAILED;

    dentry_cache_put(key, true, true, 0);
    return FS_OK;
}

/** @brief Removes an empty directory, using the dentry cache to fail fast on a known-absent path and invalidating its subtree on success. */
FsResult_t remove_dir(const char *path)
{
    if (!path) return FS_ERR_INVALID_ARG;
    if (!g_fs_mounted) return FS_ERR_NOT_MOUNTED;

    std::string key;
    dentry_normalize(path, key);

    DentryEntry cached;
    if (dentry_cache_lookup(key, cached) && !cached.exists) {
        return FS_ERR_NOT_FOUND;
    }
    if (!LittleFS.exists(path)) {
        dentry_cache_put(key, false, false, 0);
        return FS_ERR_NOT_FOUND;
    }

    if (!LittleFS.rmdir(path)) return FS_ERR_OPERATION_FAILED;

    // rmdir() only ever succeeds on an empty directory, so there should be no
    // cached descendants left to worry about - but invalidate the whole
    // subtree anyway rather than rely on that invariant holding forever.
    dentry_cache_invalidate_subtree(key);
    return FS_OK;
}
