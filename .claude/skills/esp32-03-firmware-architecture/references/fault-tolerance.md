# Lưới an toàn: watchdog, panic, crash recovery

Skill `esp32-07-debugging` xử lý lỗi **đã xảy ra** và **cấm** tắt/nới watchdog.
File này là phía đối diện: **thiết kế lưới an toàn ngay từ đầu** để khi lỗi xảy ra ngoài
thực địa, thiết bị tự về trạng thái dùng được và để lại đủ dấu vết.

Áp dụng theo mức dự án (`tiers.md`): simple → chỉ mục 1; medium → 1-3; production → toàn bộ.

## 1. Watchdog — ai bật, ai feed

ESP-IDF có ba lớp, đừng nhầm:

| Loại | Bắt cái gì | Cấu hình |
|---|---|---|
| **TWDT** (Task WDT) | task chạy mãi không nhường CPU, vòng lặp kẹt | `CONFIG_ESP_TASK_WDT_*` |
| **IWDT** (Interrupt WDT) | ISR hoặc critical section giữ CPU quá lâu | `CONFIG_ESP_INT_WDT_*` |
| **RTC WDT** | treo trong bootloader, trước khi RTOS chạy | bootloader config |

Quy tắc thiết kế:

- **Bật TWDT ở mọi dự án medium trở lên.** `CONFIG_ESP_TASK_WDT_INIT=y`, panic khi timeout
  (`CONFIG_ESP_TASK_WDT_PANIC=y`) — reset im lặng không để lại gì để chẩn đoán.
- **Chỉ subscribe task có vòng lặp giới hạn thời gian biết trước.** Task blocking trên queue
  với timeout dài thì đừng subscribe — nó sẽ báo động giả và người ta sẽ tắt watchdog.
- **Mỗi task tự feed đúng một chỗ**: ở đầu vòng lặp chính, sau khi đã qua điểm chờ.
  Không rải `esp_task_wdt_reset()` giữa vòng lặp để "cho qua" — đó là hành vi bị cấm ở 07.
- **Timeout phải có cơ sở**: bằng ~3× chu kỳ chậm nhất mà vòng lặp đó có thể hợp lệ.
  Ghi lý do vào comment. Con số không giải thích được là con số sẽ bị nới khi có sự cố.
- Task nào bị TWDT bắt là dữ liệu chẩn đoán quý nhất — đảm bảo tên task có nghĩa.

## 2. Trạng thái an toàn khi panic

Panic xảy ra ở ngữ cảnh rất hạn chế: không cấp phát, không chờ bus, không log dài.

- Cơ cấu chấp hành phải về trạng thái an toàn bằng **phần cứng** (pull-down/pull-up trên chân
  điều khiển) — vì lúc reset chân ở trạng thái float. Không dựa vào code chạy kịp.
  Chi tiết: `esp32-04-driver-development/references/actuators.md`.
- Nếu có `esp_register_shutdown_handler()`: chỉ làm việc ngắn, không blocking, không cấp phát.
  Nó chạy ở `esp_restart()` có trật tự, **không** chạy ở mọi panic.
- Đừng cố "cứu" trong panic handler. Việc của nó là để lại dấu vết, không phải sửa lỗi.

## 3. Bộ đếm crash và safe mode

Không có cái này thì một bản firmware lỗi + boot loop = thiết bị chết ngoài thực địa.

```c
/* Trong app_main, TRƯỚC khi khởi tạo bất cứ thứ gì có thể crash */
uint32_t boot_fail = nvs_get_boot_fail_count();   /* tăng và ghi ngay */
if (boot_fail >= 3) {
    enter_safe_mode();     /* chỉ bật: nguồn, console, mạng tối thiểu, OTA. Không chạy app */
}
/* ... init đầy đủ ... */
/* Chạy ổn định N giây (ví dụ 60 s) rồi mới xoá bộ đếm — không xoá ngay đầu app_main */
```

- **Ngưỡng xoá bộ đếm là "chạy ổn định N giây", không phải "boot xong".** Xoá ngay đầu
  `app_main` làm bộ đếm vô dụng với crash xảy ra sau 5 giây.
- Safe mode phải đủ để **OTA được** — nếu không thì nó chỉ là một cách chết chậm hơn.
- Ghi vào NVS: lý do reset gần nhất (`esp_reset_reason()`), phiên bản firmware, số lần crash.
  Đây là input của `esp32-12-release` (chẩn đoán từ xa).
- Quan hệ với OTA rollback: rollback của IDF chỉ bắt được bản **vừa cập nhật**. Bộ đếm này bắt
  cả những lỗi phát sinh sau đó (NVS hỏng, cấu hình xấu, phần cứng suy giảm).

## 4. Factory reset

Mọi thiết bị production cần một đường đưa về mặc định mà **không cần cáp**:

- Cách kích hoạt: giữ nút N giây lúc boot, hoặc lệnh từ server đã xác thực.
- Xoá cái gì: cấu hình người dùng, credential. **Không** xoá dữ liệu hiệu chuẩn nhà máy và
  danh tính thiết bị — để hai loại ở **namespace NVS khác nhau** ngay từ đầu.
- Phải có xác nhận (giữ nút đủ lâu + LED báo), không kích hoạt bằng một lần nhấn.

## 5. Kiểm chứng

Lưới an toàn không được test = lưới an toàn không tồn tại. Tối thiểu:

- Ép một task treo (`while(1);`) → TWDT phải bắt, log phải nêu đúng tên task.
- Ép boot loop 3 lần → phải vào safe mode, và **phải OTA được từ safe mode**.
- Cắt nguồn giữa lúc ghi NVS → boot lại phải vào trạng thái hợp lệ.

Chuyển sang `esp32-08-testing` để hiện thực (fault injection → `esp32-08-testing/references/fault-injection.md`).
