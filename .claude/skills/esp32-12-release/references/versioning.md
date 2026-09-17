# Phiên bản và build production

## Vì sao phải nghiêm túc

Thiết bị ngoài thực địa gửi về một báo cáo lỗi. Câu hỏi đầu tiên luôn là: **bản nào?**
Không trả lời được thì mọi phân tích sau đó là đoán.

## Nguồn phiên bản: một, không phải ba

Sai lầm phổ biến: số phiên bản nằm ở `#define FW_VERSION`, ở tên file binary, và ở tag git —
ba nơi lệch nhau. Chọn **một nguồn duy nhất**, các nơi khác suy ra từ đó.

Cách khuyến nghị với ESP-IDF: để build system tự lấy từ git.

```cmake
# CMakeLists.txt gốc, TRƯỚC project()
set(PROJECT_VER_FROM_GIT 1)     # hoặc: set(PROJECT_VER "1.4.0")
```

IDF ghi giá trị này vào `esp_app_desc_t` trong ảnh firmware. Đọc ở runtime:

```c
const esp_app_desc_t *d = esp_app_get_description();   /* IDF v5.x */
ESP_LOGI(TAG, "fw %s built %s %s idf %s",
         d->version, d->date, d->time, d->idf_ver);
```

`esp_app_desc_t` còn dùng để **chặn nạp nhầm firmware của sản phẩm khác** khi OTA
(so `project_name`) — xem `esp32-10-security/references/ota-security.md`.

## Quy tắc đánh số

`MAJOR.MINOR.PATCH`, ý nghĩa cho firmware:

| Phần | Tăng khi |
|---|---|
| MAJOR | đổi giao thức/schema dữ liệu theo cách bản cũ không hiểu, hoặc đổi partition |
| MINOR | thêm tính năng, vẫn tương thích ngược |
| PATCH | sửa lỗi |

Bổ sung riêng cho firmware:
- Build không sạch (có thay đổi chưa commit) phải có hậu tố `-dirty`. Firmware `-dirty`
  **không bao giờ** được phát hành OTA — không tái tạo lại được để chẩn đoán.
- `secure_version` cho anti-rollback là **số khác**, chỉ tăng khi vá lỗ hổng, và đốt eFuse.
  Đừng gắn nó vào PATCH. Chi tiết: `esp32-10-security/references/ota-security.md`.
- Mỗi bản phát hành ra ngoài phải có **tag git tương ứng** và ELF được lưu lại — thiếu ELF
  thì backtrace từ thực địa không giải mã được (`esp32-07-debugging`).

## Ghi phiên bản vào NVS

Binary tự biết phiên bản của nó, nhưng NVS cần lưu **lịch sử**:

```c
/* Sau khi self-test OTA thành công */
nvs_set_str(h, "fw_cur",  cur_version);
nvs_set_str(h, "fw_prev", prev_version);   /* để biết vừa nâng từ đâu lên */
nvs_set_u32(h, "fw_since", (uint32_t) time(NULL));
```

Dùng để: phân biệt lỗi mới xuất hiện sau bản nào, và biết bản cũ là gì khi cần quay lui thủ công.

## Build production khác build dev ở đâu

Phải liệt kê được **toàn bộ** khác biệt. Không khác biệt nào được là "tình cờ".

| Hạng mục | Dev | Production |
|---|---|---|
| Mức log | DEBUG / VERBOSE | INFO trở xuống |
| `assert` | bật | cân nhắc — tắt tiết kiệm flash nhưng mất lưới an toàn |
| Console / monitor | bật | thường tắt |
| Endpoint server | staging | production |
| Partition | có thể khác | **đúng bảng sẽ ship**, không đổi về sau |
| Core dump | UART tiện hơn | vào flash (`remote-diagnostics.md`) |
| Secure Boot / Flash Enc | tắt hoặc Development | theo `esp32-10-security` |

Cách làm: hai file `sdkconfig.defaults` + `sdkconfig.prod`, chọn bằng biến môi trường trong CI.
Không sửa tay `sdkconfig` rồi ship — `sdkconfig` không nên nằm trong git
(`esp32-01-project-init`).

**Xác minh bằng cách đọc lại binary**, không bằng cách tin vào quy trình: build xong, chạy thử,
kiểm mức log thực tế và endpoint thực tế trước khi phát hành.

## Danh sách kiểm nhanh trước khi gắn tag

- [ ] Working tree sạch, không `-dirty`.
- [ ] Phiên bản trong `esp_app_desc_t` đúng bằng tag git.
- [ ] ELF + map file đã lưu vào nơi truy được sau này.
- [ ] `idf.py size` còn biên so với slot OTA (`esp32-09/references/binary-size.md`).
- [ ] Khác biệt dev/production đã liệt kê đủ và đã xác minh trên binary thật.
