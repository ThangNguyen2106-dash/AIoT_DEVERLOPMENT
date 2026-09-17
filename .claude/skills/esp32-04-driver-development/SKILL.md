---
name: esp32-04-driver-development
description: Viết driver ngoại vi và driver thiết bị cho ESP32 — GPIO, UART, I2C, SPI, RS485, Modbus RTU, CAN/TWAI, PWM, ADC, cảm biến, màn hình, cơ cấu chấp hành. Theo quy trình Requirement → Interface → Init → Config → Read/Write → Timeout → Error → Recovery → Logging → Test. Dùng khi cần đọc cảm biến, điều khiển chân, cấu hình bus, bọc module ngoài thành component tái sử dụng, hoặc khi giao tiếp phần cứng không hoạt động như mong đợi.
---

# 04 — Driver Development

## Điều kiện vào
Chưa rõ chip / framework / phiên bản IDF → chạy Context Gate ở `esp32-firmware` trước.
Cổng an toàn và ngân sách context của `esp32-firmware` áp dụng nguyên vẹn tại đây.

Tầng chạm phần cứng: code gọi thẳng ngoại vi ESP-IDF, và component bọc từng chip/module ngoài.
Đầu ra là component có header sạch, không chứa nghiệp vụ, dùng lại được ở dự án khác.

## Nguyên tắc tối thượng: trừu tượng vừa đủ

Driver đơn giản thì viết đơn giản. Một chân relay không cần factory, không cần bảng vtable,
không cần lớp HAL riêng. Chỉ thêm một lớp trừu tượng khi nói được **nó chặn cái đau cụ thể nào**
(đổi chip cảm biến, hai biến thể board, cần mock để test trên host).

Ngược lại, dù driver nhỏ đến đâu vẫn phải giữ đủ 6 điều bắt buộc ở dưới.

## 6 điều bắt buộc với mọi driver

1. **Không chứa business logic.** Driver trả số liệu và trạng thái; ngưỡng cảnh báo, lịch đo,
   quyết định bật/tắt thuộc `esp32-06-application-development`.
2. **Interface rõ ràng.** Header nói được: gọi từ context nào (task/ISR), có blocking không,
   blocking tối đa bao lâu, ai sở hữu bộ nhớ, trả mã lỗi gì.
3. **Có timeout ở mọi thao tác chờ phần cứng.** Không `portMAX_DELAY`, không vòng `while` chờ
   cờ mà không có lối thoát.
4. **Xử lý lỗi giao tiếp.** Phân biệt lỗi tham số, lỗi timeout, lỗi CRC/checksum, lỗi thiết bị
   không phản hồi — trả mã khác nhau, đừng gộp thành `ESP_FAIL`.
5. **Tránh blocking không cần thiết.** Chờ dài (ví dụ đợi cảm biến chuyển đổi 750 ms) không
   được busy-wait; trả về `ESP_ERR_NOT_FINISHED` hoặc tách `start()` / `read()`.
6. **Trạng thái lỗi rõ ràng.** Driver biết mình đang UNINIT / READY / ERROR / RECOVERING và
   cho tầng trên truy vấn được, thay vì im lặng trả dữ liệu cũ.

## Quy trình 10 bước

Chạy tuần tự. Mỗi bước có đầu ra cụ thể; không nhảy thẳng vào code.

| # | Bước | Đầu ra | Bẫy thường gặp |
|---|---|---|---|
| 1 | **Requirement** | bus, tốc độ, chu kỳ đọc, độ chính xác, số thiết bị, môi trường | đoán tần số lấy mẫu; bỏ qua độ dài dây |
| 2 | **Interface** | header công khai + hợp đồng dưới đây | thiết kế API quanh datasheet thay vì quanh người dùng |
| 3 | **Initialization** | `*_create()` cấp tài nguyên, kiểm tra thiết bị có thật (đọc chip ID) | init "thành công" dù chưa cắm thiết bị |
| 4 | **Configuration** | pin/tham số nhận qua struct config | hard-code chân, hard-code địa chỉ |
| 5 | **Read/Write** | hàm đọc/ghi + hàm chuyển đổi raw sang đơn vị (thuần, tách riêng) | trộn tính toán vào hàm I/O nên không test được |
| 6 | **Timeout** | mọi lời gọi bus có timeout, có giá trị lấy từ datasheet | `portMAX_DELAY`; timeout đặt bừa 1000 ms |
| 7 | **Error handling** | bảng mã lỗi → ý nghĩa; validate dữ liệu đọc về (CRC, dải hợp lệ) | trả giá trị rác vì không kiểm tra CRC |
| 8 | **Recovery** | retry có giới hạn, reset thiết bị, reset bus, báo lên trên khi hết cách | retry vô hạn trong driver, chặn cả task |
| 9 | **Logging** | `TAG` riêng, lỗi ở mức E/W, chi tiết ở mức D | log INFO mỗi lần đọc; log trong ISR |
| 10 | **Test** | tự kiểm tra cơ bản + điểm móc để `esp32-08-testing` viết test | không có cách nào kiểm tra khi thiếu phần cứng |

Chi tiết từng bước 2, 6, 7, 8, 9, 10 → `references/driver-contract.md`.

## Hợp đồng API bắt buộc

```c
/* device.h — handle-based, opaque, nhiều instance */
typedef struct device_s device_t;

typedef struct {
    i2c_master_bus_handle_t bus;   /* hoặc spi_host, uart_port... do tầng trên sở hữu */
    uint8_t  addr;
    uint32_t timeout_ms;
} device_config_t;

/* Gọi từ task. Blocking tối đa timeout_ms. */
esp_err_t device_create(const device_config_t *cfg, device_t **out);
esp_err_t device_read(device_t *dev, device_data_t *out);
esp_err_t device_get_state(device_t *dev, device_state_t *out);
esp_err_t device_recover(device_t *dev);
void      device_destroy(device_t *dev);
```

Quy ước: driver **không** tự tạo task, không tự tạo bus (nhận bus từ ngoài), không giữ biến
static toàn cục cho instance, không tự retry quá số lần đã khai báo trong config.

## Định tuyến reference — đọc đúng file đang cần, không đọc cả thư mục

| Cần làm | Reference |
|---|---|
| Hợp đồng interface, timeout, lỗi, recovery, state, log, test | `references/driver-contract.md` |
| Chân số, ngắt ngoài, chống dội phím, open-drain, pull-up | `references/gpio.md` |
| I2C master, quét bus, nhiều thiết bị chung bus | `references/i2c.md` |
| SPI, DMA, nhiều slave, tốc độ cao | `references/spi.md` |
| UART, parser theo frame, event queue | `references/uart.md` |
| RS485 half-duplex, Modbus RTU master/slave | `references/rs485-modbus.md` |
| CAN / TWAI, bit timing, bus-off recovery | `references/twai-can.md` |
| LEDC, MCPWM, servo, cầu H | `references/pwm-motor.md` |
| ADC oneshot/continuous, hiệu chuẩn, lọc nhiễu | `references/adc.md` |
| Bọc cảm biến thành component, chuyển đổi đơn vị, hiệu chuẩn | `references/sensors.md` |
| LCD ký tự, OLED, TFT, e-paper, LVGL | `references/displays.md` |
| Relay, solenoid, van, stepper, an toàn cơ cấu chấp hành | `references/actuators.md` |

## Mẫu báo cáo trước khi viết code

```
## Thiết bị
<tên chip/module> — bus <I2C/SPI/UART/...> — địa chỉ/CS <...> — datasheet <phần tham chiếu>

## Ràng buộc từ datasheet
| Tham số | Giá trị | Ảnh hưởng tới driver |
|---|---|---|
| thời gian chuyển đổi | 750 ms | tách start/read, không busy-wait |
| tốc độ bus tối đa | 400 kHz | ... |
| thời gian sau power-on | 2 ms | delay trong create() |

## Interface đề xuất
<khối header rút gọn>

## Timeout và lỗi
| Thao tác | Timeout | Lỗi có thể | Xử lý |
|---|---|---|---|
| read reg | 100 ms | ESP_ERR_TIMEOUT | retry 2 lần rồi trả lên |
| CRC sai  | —      | ESP_ERR_INVALID_CRC | bỏ mẫu, đếm, 3 lần liên tiếp → ERROR |

## Chiến lược recovery
<retry → reset thiết bị → reset bus → báo lên trên>

## Điểm test
<kiểm tra được gì khi không có phần cứng; cần treo thiết bị gì để test thật>
```

## Không thuộc scope

- Chọn chip, chọn chân, chân cấm, xung đột ngoại vi → `esp32-02-hardware-analysis`
- Driver chạy trong task nào, đồng bộ giữa task → `esp32-03-firmware-architecture`
- Ngưỡng, lịch đo, state machine nghiệp vụ, lưu NVS → `esp32-06-application-development`
- Wi-Fi/BLE/MQTT/HTTP, kể cả khi dữ liệu đến từ driver → `esp32-05-connectivity`
- Bus im lặng, crash, treo đã xảy ra trên board thật → `esp32-07-debugging`
- Viết unit test / HIL cho driver → `esp32-08-testing`
- Giảm thời gian bận, tối ưu DMA, throughput bus → `esp32-09-performance-optimization`

## Đầu ra
Component hoàn chỉnh: `include/<dev>.h` (có comment hợp đồng), `<dev>.c`, `CMakeLists.txt`,
`README.md` ngắn nêu cách nối dây và ví dụ dùng — kèm báo cáo theo mẫu trên.
