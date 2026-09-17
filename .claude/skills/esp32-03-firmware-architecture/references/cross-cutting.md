# Quy ước xuyên suốt: configuration, logging, storage, OTA

Bốn thứ này chạm mọi lớp. Chốt một lần ở đầu dự án; sửa về sau rất đắt.

## Configuration

Phân theo **thời điểm giá trị được biết**:

| Loại | Nơi chứa | Ví dụ | Đổi được lúc chạy? |
|---|---|---|---|
| Board/phần cứng | `board_config.h` | chân GPIO, địa chỉ I2C, tần số bus | không |
| Build-time tuỳ chọn | Kconfig (`sdkconfig`) | bật/tắt tính năng, kích thước buffer, log level | không |
| Runtime per-device | NVS | SSID, token, hệ số hiệu chuẩn, serial | có |
| Runtime tạm | RAM | trạng thái kết nối, bộ đếm | mất khi reset |

Quy tắc:
- Một nguồn sự thật cho mỗi giá trị. Không `#define` trùng ở hai file.
- Bí mật (khoá, token, mật khẩu) **không bao giờ** nằm trong source hay git → NVS,
  nạp lúc sản xuất. Chi tiết → `esp32-10-security`.
- Component nhận cấu hình qua struct lúc `init()`, không tự đọc NVS ở giữa lớp sâu.
  Một nơi đọc cấu hình (`config_svc`), rồi phân phát xuống.
- Mọi giá trị đọc từ NVS phải có **mặc định an toàn** khi key chưa tồn tại hoặc hỏng.
  Thiết bị mới ra khỏi xưởng phải boot được với NVS rỗng.
- Cấu hình pin cụ thể → `esp32-02-hardware-analysis`; cây Kconfig/partition →
  `esp32-01-project-init`.

## Logging

Câu hỏi quyết định: **ai đọc log, đọc ở đâu?** Lúc dev là bạn qua UART. Ngoài thực địa
thì không ai cắm cáp — nên thứ không lưu lại được thì coi như không tồn tại.

- Mỗi file một `static const char *TAG`. Không `printf`.
- Quy ước mức, dùng nhất quán toàn dự án:
  - `E` — thiết bị không làm đúng chức năng; cần người can thiệp hoặc fail-safe.
  - `W` — bất thường nhưng đã tự xử lý (retry thành công, dùng giá trị mặc định).
  - `I` — mốc vòng đời: boot, phiên bản, kết nối, transition trạng thái quan trọng.
  - `D`/`V` — chi tiết gỡ lỗi; mặc định tắt trong build production.
- Log ở tầng **quyết định**, không ở mọi tầng. Một lỗi đi qua 3 lớp mà lớp nào cũng log
  thì đọc log không ra được chuyện gì. Lớp dưới trả lỗi lên, lớp quyết định log một lần
  kèm ngữ cảnh.
- Không log trong ISR, không log trong vòng lặp nóng (nghẽn UART, lệch timing).
- Production: log mức chỉnh được lúc chạy (`esp_log_level_set`) và lỗi nghiêm trọng
  lưu được để lấy về sau (NVS ring buffer nhỏ, hoặc core dump →
  `esp32-12-release`).

## Storage

Phân theo **cái gì phải sống qua cái gì**:

| Dữ liệu | Sống qua reset | Sống qua OTA | Nơi chứa |
|---|---|---|---|
| Cấu hình, hiệu chuẩn, danh tính | có | có | NVS |
| Đệm dữ liệu chờ gửi | tuỳ | không cần | RAM, hoặc file nếu lớn |
| File lớn (web asset, log, model) | có | có | SPIFFS/LittleFS/SD |
| Trạng thái qua deep sleep | có | — | RTC memory (`esp32-11-power-management`) |

Quy tắc:
- NVS chia namespace theo module; đừng dồn hết vào một namespace.
- **Schema có version**: ghi một key `schema_ver`. Khi OTA đổi cấu trúc, đọc version
  rồi migrate. Không có nó thì bản cập nhật đầu tiên sẽ đọc rác.
- Ghi flash là thao tác đắt và có giới hạn số chu kỳ. Không ghi trong vòng lặp;
  gom lại, ghi khi giá trị thực sự đổi.
- Mọi lần đọc phải xử lý được: key không tồn tại, dữ liệu sai kích thước, dữ liệu hỏng.
  Hỏng → dùng mặc định + log WARN, không crash.
- Phân vùng NVS/SPIFFS trong partition table → `esp32-01-project-init`.

## OTA

Quyết định có OTA hay không phải nằm ở **đầu** dự án, vì nó ràng buộc partition table,
kích thước binary, và storage schema. Thêm OTA vào sau thường phải nạp lại toàn bộ
thiết bị bằng tay.

Ảnh hưởng kiến trúc (thiết kế ở skill này, triển khai ở `esp32-12-release`):
- Partition: cần `factory` + `ota_0` + `ota_1` + `otadata` → binary chỉ được dùng
  khoảng một nửa flash khả dụng. Ràng buộc này chi phối lựa chọn thư viện.
- Cập nhật phải **nguyên tử**: hoặc bản mới chạy, hoặc quay về bản cũ. Không có
  trạng thái nửa vời.
- Phải có **rollback tự động**: bản mới tự xác nhận (`esp_ota_mark_app_valid_cancel_rollback`)
  sau khi qua self-test; không xác nhận được thì bootloader quay về bản cũ.
- Self-test sau cập nhật phải kiểm thứ đúng cái hay hỏng: kết nối được mạng, đọc được
  cảm biến, NVS đọc được. Không chỉ "boot xong".
- Task nào có thể đang giữ tài nguyên khi OTA chạy? Chốt trước: OTA task priority thấp,
  và các task khác phải chịu được việc thiết bị reset bất cứ lúc nào.
- Dữ liệu người dùng phải sống qua OTA → nằm ở NVS/partition riêng, không nằm trong
  app partition.
- Ký firmware và Secure Boot → `esp32-10-security`.
