# File System API Specification (Frontend <-> Backend Contract)

> **Trạng thái (Status)**: 
> - **Frontend UI**: Đã hoàn thành tại [`include/html/manage_file_system_html.h`](include/html/manage_file_system_html.h) và được route tại `/tools/manage_file_system`.
> - **Backend API**: **CHƯA TRIỂN KHAI (PENDING)** — Tài liệu này mô tả chi tiết các endpoint cần bổ sung vào backend (ví dụ trong `src/core/web_server_task.cpp`).

---

## 1. Danh sách các API Endpoint cần triển khai ở Backend

| STT | Endpoint | Method | Chức năng | Request Format | Response Format |
| :--- | :--- | :---: | :--- | :--- | :--- |
| 1 | `/api/fs/list` | `GET` | Lấy dung lượng bộ nhớ & danh sách file | None | `application/json` |
| 2 | `/api/fs/read` | `GET` | Đọc nội dung 1 file để hiển thị lên Editor | Query `path` | `text/plain` |
| 3 | `/api/fs/save` | `POST` | Lưu nội dung chỉnh sửa hoặc tạo file mới | `application/json` | `text/plain` (`OK` / `FAIL`) |
| 4 | `/api/fs/delete` | `POST` | Xóa một file khỏi LittleFS | `application/json` | `text/plain` (`OK` / `FAIL`) |

---

## 2. Chi tiết từng Endpoint

### 2.1. Lấy danh sách file và dung lượng bộ nhớ: `GET /api/fs/list`

* **Mô tả**: Frontend gọi endpoint này khi vừa mở trang hoặc khi bấm nút **"↻ Refresh"**.
* **Method**: `GET`
* **URL**: `/api/fs/list`
* **Yêu cầu xác thực**: HTTP Basic Auth (admin / password)

#### Response:
* **Content-Type**: `application/json`
* **HTTP Status**: `200 OK`
* **Cấu trúc JSON trả về**:
```json
{
  "totalBytes": 1441792,
  "usedBytes": 28672,
  "files": [
    {
      "name": "/web_config.txt",
      "size": 120
    },
    {
      "name": "/wifi_config.txt",
      "size": 85
    },
    {
      "name": "/sys_config_registry.txt",
      "size": 64
    }
  ]
}
```

---

### 2.2. Đọc nội dung file: `GET /api/fs/read`

* **Mô tả**: Frontend gọi endpoint này khi người dùng bấm vào tên file hoặc bấm nút **"Edit"**.
* **Method**: `GET`
* **URL**: `/api/fs/read?path=<file_path>`
* **Query Parameters**:
  * `path` (bắt buộc): Đường dẫn file cần đọc (ví dụ: `/web_config.txt`).

#### Ví dụ Request URL:
```http
GET /api/fs/read?path=%2Fweb_config.txt
```

#### Response:
* **Content-Type**: `text/plain`
* **HTTP Status**:
  * `200 OK` nếu đọc thành công.
  * `404 Not Found` nếu file không tồn tại.
* **Body**: Toàn bộ chuỗi nội dung văn bản thô (raw string) của file.

---

### 2.3. Lưu file (Chỉnh sửa / Tạo mới): `POST /api/fs/save`

* **Mô tả**: Frontend gọi endpoint này khi người dùng bấm nút **"💾 Save"** hoặc phím tắt <kbd>Ctrl</kbd>+<kbd>S</kbd>.
* **Method**: `POST`
* **URL**: `/api/fs/save`
* **Request Header**: `Content-Type: application/json`

#### Request Payload (JSON Body):
```json
{
  "path": "/web_config.txt",
  "content": "port=80\nusername=admin\npassword=admin\n"
}
```

#### Response:
* **Content-Type**: `text/plain`
* **HTTP Status**:
  * `200 OK` kèm body `"OK"` khi ghi file thành công.
  * `400 Bad Request` nếu payload lỗi hoặc thiếu tham số.
  * `500 Internal Server Error` nếu ghi file thất bại.

---

### 2.4. Xóa file: `POST /api/fs/delete`

* **Mô tả**: Frontend gọi endpoint này khi người dùng bấm nút **"Del"** trên danh sách file.
* **Method**: `POST`
* **URL**: `/api/fs/delete`
* **Request Header**: `Content-Type: application/json`

#### Request Payload (JSON Body):
```json
{
  "path": "/temp.txt"
}
```

#### Response:
* **Content-Type**: `text/plain`
* **HTTP Status**:
  * `200 OK` kèm body `"OK"` khi xóa file thành công.
  * `404 Not Found` hoặc `500 Internal Server Error` nếu file không tồn tại hoặc lỗi khi xóa.
