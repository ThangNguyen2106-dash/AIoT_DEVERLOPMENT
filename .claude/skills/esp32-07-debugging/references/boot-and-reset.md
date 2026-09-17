# Boot failure, reset reason, brownout

Phục vụ lớp lỗi: không boot, boot loop, reset ngẫu nhiên, reset do nguồn.

## Bước đầu tiên luôn là: vì sao reset?

Dòng đầu tiên sau mỗi lần reset cho biết hướng điều tra. **Không đọc dòng này thì mọi phân tích
sau đều là đoán.**

| Chuỗi trong log | Ý nghĩa | Hướng điều tra |
|---|---|---|
| `rst:0x1 (POWERON_RESET)` | bật nguồn — hoặc nguồn chập chờn/sụt sâu | nếu xảy ra khi đang chạy: lỗi nguồn, xem mục Brownout |
| `rst:0x3 (SW_RESET)` | phần mềm gọi `esp_restart()` | tìm chỗ gọi restart; có OTA/lỗi được "xử lý" bằng reboot không |
| `rst:0x5 (DEEPSLEEP_RESET)` | thức từ deep sleep | bình thường; nếu ngoài ý muốn → `esp32-11-power-management` |
| `rst:0x7 (TG0WDT_SYS_RESET)` | watchdog timer group 0 | `rtos-faults.md` |
| `rst:0x8 (TG1WDT_SYS_RESET)` | watchdog timer group 1 / task watchdog | `rtos-faults.md` |
| `rst:0x9 (RTCWDT_SYS_RESET)` | RTC watchdog — thường treo trong giai đoạn boot | xem mục Boot failure |
| `rst:0xc (SW_CPU_RESET)` | **panic**: exception, `abort()`, assert | `panic-backtrace.md` |
| `rst:0xd (RTCWDT_RTC_RESET)` | RTC WDT ở mức hệ thống | nguồn hoặc treo sớm khi boot |
| `rst:0x10 (RTCWDT_RTC_RESET)` | thường đi kèm brownout / nguồn không ổn định | xem mục Brownout |
| `rst:0x15 (USB_UART_CHIP_RESET)` | host USB reset chip | bình thường khi cắm/rút; không phải bug |
| `Brownout detector was triggered` | điện áp tụt dưới ngưỡng | **lỗi phần cứng**, xem mục Brownout |

Tên mã reset khác nhau đôi chút giữa các dòng chip (ESP32 / S2 / S3 / C3 / C6) —
**tra đúng ESP-IDF của target đang dùng**, không suy từ trí nhớ.

Đọc trong code:

```c
#include "esp_system.h"
esp_reset_reason_t r = esp_reset_reason();   /* ESP_RST_PANIC, ESP_RST_TASK_WDT, ESP_RST_BROWNOUT, ... */
```

In lý do reset ngay đầu `app_main` là việc đáng làm cho mọi firmware sẽ ra thực địa.

## Phân biệt ba thứ hay bị gộp làm một

| | Có panic log? | Có reset? | Hướng đi |
|---|---|---|---|
| **Crash** | có (Guru Meditation / abort) | có | `panic-backtrace.md` |
| **Reset** | không | có | bảng reset reason ở trên |
| **Treo (hang)** | không | **không** | `rtos-faults.md` — bật TWDT để biến treo thành log |

Người dùng nói "nó bị reset" thường không phân biệt được ba thứ này. Hỏi lại:
*"Sau khi lỗi, log có in lại từ đầu (`rst:`) không, hay đứng im không ra chữ nào nữa?"*

## Boot failure — treo hoặc lặp trước `app_main`

Trình tự boot bình thường: ROM bootloader → second-stage bootloader (`boot:` log) →
app khởi động (`cpu_start:`) → `app_main`. Xác định **treo ở giai đoạn nào** trước.

| Hiện tượng | Nguyên nhân thường gặp | Xác minh |
|---|---|---|
| Chỉ có `ESP-ROM:...` rồi im | chip vào download mode do strapping, hoặc flash không đọc được | kiểm tra GPIO0/GPIO9 lúc reset; `esptool.py flash_id` |
| `waiting for download` | GPIO0 bị kéo thấp | tháo mạch trên GPIO0, xem `esp32-02-hardware-analysis` |
| `invalid header: 0xffffffff` | flash trống hoặc offset app sai | nạp lại; đối chiếu partition table |
| `ota_data partition invalid`, chọn sai slot | OTA hỏng, rollback chưa cấu hình | `esp32-12-release` |
| `Image checksum/signature failed` | firmware hỏng, sai khoá ký, flash lỗi | nạp lại; nếu bật Secure Boot → `esp32-10-security` |
| Boot loop **y hệt** mỗi vòng | lỗi tất định trong code khởi tạo | thu hẹp bằng cách log từng bước init |
| Boot loop **khác nhau** mỗi vòng | nguồn / flash không ổn định | đo 3V3, đổi cáp, đổi nguồn |
| `flash read err, 1000` | điện áp flash sai (1.8V vs 3.3V), hoặc GPIO12 bị kéo cao trên ESP32 | `esp32-02-hardware-analysis` |
| Treo sau `cpu_start` trước `app_main` | init component nặng, PSRAM không có thật, khai báo tĩnh quá lớn | kiểm tra cấu hình PSRAM và flash size |
| Reset liên tục sau vài giây, không panic | RTC WDT: bootloader chạy xong nhưng app treo sớm | thêm log ngay dòng đầu `app_main` |

**Phép thử tách phần cứng khỏi phần mềm:** nạp một firmware `hello_world` sạch.
- Chạy được → lỗi nằm trong code/cấu hình của dự án.
- Vẫn không boot → lỗi phần cứng, flash, hoặc nguồn → `esp32-02-hardware-analysis`.

Đây là phép thử rẻ nhất và nên làm sớm khi nghi boot failure.

## Bật log bootloader chi tiết

Khi log boot quá ít để kết luận:

```
CONFIG_BOOTLOADER_LOG_LEVEL_DEBUG=y
CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y
```

Baudrate console mặc định 115200; nếu log ra ký tự rác, kiểm tra crystal 26MHz vs 40MHz
(ESP32 cũ) hoặc baudrate monitor sai — **rác lúc boot ở 74880 là log ROM bình thường**,
không phải lỗi.

## Brownout và reset do nguồn

`Brownout detector was triggered` nghĩa là điện áp lõi tụt dưới ngưỡng. Đây là **lỗi điện**, không
phải lỗi phần mềm. Tắt detector không sửa gì — chỉ khiến chip chạy ở vùng điện áp không đảm bảo,
nơi flash có thể bị ghi/đọc sai và dữ liệu NVS hỏng.

Dấu hiệu nhận diện:

| Quan sát | Kết luận |
|---|---|
| Reset đúng lúc relay/motor/LED công suất bật | sụt áp do dòng đỉnh của tải |
| Reset khi Wi-Fi bắt đầu phát (đặc biệt lúc connect) | dòng đỉnh RF; nguồn hoặc tụ không đủ |
| Chỉ lỗi với cáp USB này, đổi cáp thì hết | sụt áp trên cáp/đầu nối |
| Chỉ lỗi khi cấp qua chân 5V của board | LDO trên board không đủ dòng |
| Điện áp 3V3 đo được sụt rõ khi có tải | xác nhận |

Hướng xử lý (thuộc phần cứng, nêu để người dùng sửa đúng chỗ):
- Tăng tụ trên chân nguồn module; thêm tụ lớn gần tải đóng cắt.
- Cấp nguồn riêng cho tải công suất, chỉ dùng chung GND.
- Dùng adapter đủ dòng, dây đủ tiết diện, cáp USB ngắn và tốt.
- Tải cảm ứng (relay, motor, van) phải có diode dập / snubber.

Chi tiết điện: `esp32-02-hardware-analysis/references/power-electrical.md`.

## Reset do phần mềm tự gọi

`rst:0x3 (SW_RESET)` là firmware tự khởi động lại. Tìm mọi `esp_restart()` và hỏi: **vì sao ở đó
lại phải restart?** Restart như một cách "xử lý lỗi" là dấu hiệu bug bị che. Ghi lại nguyên nhân
vào NVS/RTC memory trước khi restart để lần sau có bằng chứng:

```c
/* trước esp_restart(): lưu lại vì sao */
RTC_NOINIT_ATTR static uint32_t s_restart_reason;   /* sống qua SW reset, mất khi mất nguồn */
```

## Ghi chú cho thiết bị đã ra thực địa

Không cắm được serial thì reset reason và core dump là bằng chứng duy nhất. Bật core dump vào
flash và báo lý do reset lần trước lên server ngay khi boot — cấu hình cụ thể ở
`esp32-12-release/references/ota.md` và `../references/tooling.md`.
