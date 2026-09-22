#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Outcome of a file_system.h operation.
 * Every access function below returns one of these instead of a bare bool/NULL,
 * so callers (e.g. the webserver) can map a failure to the right response
 * without re-checking existence/type themselves beforehand.
 */
typedef enum {
    FS_OK = 0,               /**< Operation completed successfully */
    FS_ERR_NOT_MOUNTED,      /**< file_system_init() was not called, or LittleFS.begin() failed */
    FS_ERR_NOT_FOUND,        /**< Path does not exist */
    FS_ERR_IS_DIRECTORY,     /**< Path exists but is a directory, not a file (e.g. read_file/write_file target) */
    FS_ERR_NOT_A_DIRECTORY,  /**< Path exists but is a file, not a directory (e.g. list_file target) */
    FS_ERR_ALREADY_EXISTS,   /**< Path already exists (e.g. make_directory on an existing path) */
    FS_ERR_OPEN_FAILED,      /**< LittleFS could not open the path (corrupt entry, too many open files, ...) */
    FS_ERR_ALLOC_FAILED,     /**< Heap/PSRAM allocation for a read/list buffer failed */
    FS_ERR_WRITE_INCOMPLETE, /**< Fewer bytes were written to flash than requested */
    FS_ERR_OPERATION_FAILED, /**< The underlying LittleFS call (remove/rename/mkdir/rmdir) failed */
    FS_ERR_INVALID_ARG,      /**< A required argument (path/data/out-pointer) was NULL */
} FsResult_t;

/** @brief Human-readable description of a FsResult_t, for logs or as an HTTP response body. */
const char *file_system_strerror(FsResult_t result);

bool file_system_init(bool formatonfail = true, const char *basepath = "/littlefs", uint8_t maxopenfiles = 4);
bool file_system_format();
size_t file_system_get_size();
size_t file_system_get_used();

/* file and directory interaction */

/**
 * @brief Đọc toàn bộ nội dung của một tập tin từ hệ thống tập tin LittleFS vào bộ nhớ đệm động.
 * 
 * @details Hàm này mở tập tin ở chế độ chỉ đọc, tính toán dung lượng và cấp phát bộ nhớ động
 *          (sử dụng PSRAM nếu macro SYSTEM_USES_PSRAM được định nghĩa, hoặc Heap tiêu chuẩn qua malloc)
 *          với kích thước bằng dung lượng file + 1 byte để gán ký tự kết thúc chuỗi '\0'.
 * 
 * @note HỢP ĐỒNG SỞ HỮU BỘ NHỚ (Memory Ownership Contract):
 *       Caller (bên gọi) chịu trách nhiệm hoàn toàn về việc giải phóng vùng nhớ `*out_data` thông qua hàm `free()`
 *       sau khi hoàn tất sử dụng để tránh hiện tượng rò rỉ bộ nhớ (memory leak).
 *       Trong trường hợp xảy ra lỗi hoặc tham số không hợp lệ, `*out_data` luôn được gán giá trị NULL.
 * 
 * @param[in]  path           Đường dẫn tuyệt đối của tập tin cần đọc trên LittleFS (ví dụ: "/config.json").
 * @param[out] out_data       Con trỏ trỏ tới vùng nhớ động chứa nội dung file vừa đọc được (do caller sở hữu và free).
 * @param[out] out_bytes_read Con trỏ lưu số byte dữ liệu thực tế đã đọc thành công từ tập tin.
 * @return FsResult_t Mã trạng thái kết quả (FS_OK nếu đọc thành công, hoặc các mã lỗi tương ứng: FS_ERR_NOT_MOUNTED,
 *                    FS_ERR_NOT_FOUND, FS_ERR_IS_DIRECTORY, FS_ERR_ALLOC_FAILED, v.v.).
 */
FsResult_t read_file(const char *path, char **out_data, unsigned int *out_bytes_read);
FsResult_t write_file(const char *path, const char *data, unsigned int length, unsigned int *out_bytes_written);

/**
 * @brief Duyệt và liệt kê tất cả các tập tin trong một thư mục, serialize thành chuỗi JSON dạng phẳng.
 * 
 * @details Hàm duyệt qua từng mục con trong thư mục chỉ định, lọc bỏ các thư mục con và thu thập tên cùng kích thước
 *          của các tập tin vào đối tượng JSON (định dạng: `{"filename": size_in_bytes, ...}`). Chuỗi JSON được cấp
 *          phát động trên Heap thông qua hàm malloc().
 * 
 * @note HỢP ĐỒNG SỞ HỮU BỘ NHỚ (Memory Ownership Contract):
 *       Caller (bên gọi) chịu trách nhiệm hoàn toàn về việc giải phóng vùng nhớ `*out_json` thông qua hàm `free()`
 *       sau khi sử dụng xong để tránh rò rỉ bộ nhớ (memory leak).
 *       Nếu thao tác thất bại hoặc thư mục không tồn tại, `*out_json` luôn được gán giá trị NULL.
 * 
 * @param[in]  dir_path Đường dẫn thư mục cần liệt kê trên LittleFS (ví dụ: "/").
 * @param[out] out_json Con trỏ trỏ tới vùng nhớ chuỗi JSON kết quả (do caller sở hữu và free).
 * @return FsResult_t Mã trạng thái kết quả (FS_OK nếu thành công, hoặc các mã lỗi tương ứng: FS_ERR_NOT_MOUNTED,
 *                    FS_ERR_NOT_FOUND, FS_ERR_NOT_A_DIRECTORY, FS_ERR_ALLOC_FAILED, v.v.).
 */
FsResult_t list_file(const char *dir_path, char **out_json);
FsResult_t remove_file(const char *path);
FsResult_t rename_file(const char *pathfrom, const char *pathto);
FsResult_t make_directory(const char *path);
FsResult_t remove_dir(const char *path);

#endif // !FILE_SYSTEM_H
