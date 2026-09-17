# Ràng buộc chân — GPIO, I/O, pull, interrupt, RTC

Phục vụ hạng mục #1, #5, #6, #7 của checklist.

## Chân tuyệt đối không dùng

| Chip | Chân cấm | Lý do |
|---|---|---|
| ESP32 | GPIO6–11 | nối SPI flash nội bộ — dùng là chết boot |
| ESP32-S3 (bản R8) | GPIO33–37 | PSRAM octal chiếm |
| S2/S3 | GPIO26–32 (tuỳ biến thể) | SPI flash/PSRAM — **phải tra datasheet module** |
| Mọi chip | chân không đưa ra header | tồn tại trên die nhưng không nối ra ngoài |

Danh sách chân flash/PSRAM phụ thuộc biến thể module. Xem `flash-psram.md` và đối chiếu
datasheet — **không suy từ số hiệu chân**.

## Chân input-only

**ESP32: GPIO34, 35, 36, 37, 38, 39.**
- Không làm output được.
- **Không có pull-up/pull-down nội** → bắt buộc pull ngoài nếu dùng làm input số hoặc nút nhấn.
- Thường dùng cho ADC1 và tín hiệu chỉ đọc.

S2/S3/C3/C6 không có nhóm input-only tương tự, nhưng vẫn phải tra datasheet từng chân.

## Năng lực I/O

- Dòng ra mỗi chân có giới hạn — LED phải có trở hạn dòng. Không lái relay, motor, LED công suất
  trực tiếp từ GPIO; dùng transistor/MOSFET/driver.
- Tổng dòng qua toàn bộ GPIO cũng có trần; nhiều chân cùng lái tải nhỏ vẫn có thể vượt.
- Open-drain cấu hình được (`GPIO_MODE_OUTPUT_OD`) — cần cho I2C và 1-Wire.
- Drive strength đặt được bằng `gpio_set_drive_capability()`. Tăng drive giúp cạnh dốc hơn
  nhưng gây nhiễu và overshoot; chỉ tăng khi có lý do đo được.
- Chân dùng cho tín hiệu tốc độ cao nên nằm trên IOMUX — xem `peripheral-matrix.md`.

## Pull-up / pull-down

| Tình huống | Yêu cầu |
|---|---|
| Bus I2C | pull-up **ngoài** 2.2k–10k lên 3.3V. Pull nội quá yếu cho 400kHz |
| Nút nhấn | phải có pull xác định; không để chân float |
| ESP32 GPIO34–39 | không có pull nội — bắt buộc pull ngoài |
| Chân điều khiển cơ cấu chấp hành | pull-down **ngoài** để giữ trạng thái an toàn lúc reset |
| Chân không dùng trong deep sleep | `rtc_gpio_isolate()` để tránh dòng rò |

Lúc reset và trong giai đoạn bootloader, chân ESP32 ở trạng thái float. Nếu mạch ngoài không có
pull xác định, tải có thể tự bật trong vài chục ms đầu — đây là rủi ro an toàn thật, không phải
lý thuyết. Kiểm tra mọi chân nối tới relay/motor/heater.

Pull nội **không hoạt động trong deep sleep** trừ khi chân thuộc RTC domain và được cấu hình
qua `rtc_gpio_pullup_en()` / `rtc_gpio_pulldown_en()`.

## Interrupt

- Hầu hết chân GPIO hỗ trợ ngắt (cạnh lên/xuống/cả hai/mức). Chân input-only vẫn ngắt được.
- Dùng `gpio_install_isr_service()` + `gpio_isr_handler_add()` để mỗi chân một handler,
  thay vì một ISR chung tự phân loại.
- Tín hiệu cơ khí (nút nhấn, công tắc) phải debounce. Ưu tiên debounce phần cứng (RC + Schmitt)
  cho tín hiệu quan trọng; debounce phần mềm chấp nhận được cho nút người dùng.
- Tín hiệu tần số rất cao nên dùng PCNT hoặc RMT thay vì ngắt GPIO — ngắt quá dày làm đói CPU.
- Ràng buộc code ISR (IRAM, không malloc, không log) thuộc `esp32-03-firmware-architecture`.

## RTC GPIO và wake source

Chỉ **RTC GPIO** mới đánh thức được từ deep sleep.

| Chip | Cơ chế | Ghi chú |
|---|---|---|
| ESP32 | `esp_sleep_enable_ext0_wakeup` (1 chân), `ext1` (nhiều chân) | chỉ RTC GPIO |
| S2/S3 | ext0 / ext1 | chỉ RTC GPIO |
| C3/C6 | `esp_deep_sleep_enable_gpio_wakeup` | tập chân hỗ trợ hẹp hơn |

Chọn chân không thuộc RTC domain làm wake source là **lỗi hay gặp nhất** ở thiết kế chạy pin.
Biểu hiện: thiết bị ngủ và không bao giờ dậy. Phải đối chiếu bảng RTC GPIO trong datasheet
của đúng chip trước khi chốt.

Chân giữ mức trong deep sleep cần `gpio_hold_en()` + `gpio_deep_sleep_hold_en()`, nếu không
mức ra sẽ thả float khi ngủ — nguy hiểm với cơ cấu chấp hành.

## Khi thiếu thông tin

Không kết luận một chân "dùng được" chỉ vì nó không nằm trong danh sách cấm ở trên.
Danh sách này là bộ lọc, không phải giấy phép. Với board custom hoặc module lạ:
hỏi schematic, hoặc yêu cầu người dùng xác nhận chân đó hiện nối gì.
