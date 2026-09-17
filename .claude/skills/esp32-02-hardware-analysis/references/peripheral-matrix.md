# Xung đột ngoại vi và định tuyến chân

Phục vụ hạng mục #3 của checklist.

## Số instance — bộ lọc nhanh

| Ngoại vi | ESP32 | S2 | S3 | C3 | C6 |
|---|---|---|---|---|---|
| UART | 3 | 2 | 3 | 2 | 2 (+LP UART) |
| I2C | 2 | 2 | 2 | 1 | 1 (+LP I2C) |
| SPI dùng được | 2 | 2 | 2 | 2 | 2 |
| I2S | 2 | 1 | 2 | 1 | 1 |
| TWAI/CAN | 1 | 1 | 1 | 1 | 2 |
| LEDC kênh | 16 (8 HS + 8 LS) | 8 | 8 | 6 | 6 |
| LEDC timer | 4 HS + 4 LS | 4 | 4 | 4 | 4 |
| MCPWM | 2 unit | không | 2 unit | không | 1 unit |
| RMT kênh | 8 | 4 | 8 (4 TX/4 RX) | 4 (2 TX/2 RX) | 4 |
| PCNT unit | 8 | 4 | 4 | không | 4 |
| Timer group | 2×2 | 2×2 | 2×2 | 2×1 | 2×1 |

Số liệu thay đổi theo phiên bản IDF và biến thể chip. Dùng làm **bộ lọc phát hiện thiếu hụt**;
khi sát trần thì đối chiếu TRM của đúng chip trước khi chốt.

## Quy tắc xung đột

**SPI**
- Host gắn với flash nội (SPI1 trên ESP32) **không dùng cho thiết bị ngoài**.
- Còn lại thường là SPI2/SPI3 (HSPI/VSPI trên ESP32 classic).
- Nhiều slave dùng chung một host được — thêm nhiều `spi_device`, IDF tự đổi cấu hình.
  Chỉ hết host khi cần bus vật lý độc lập.
- Chia sẻ bus với thẻ SD, LCD, flash ngoài: tổng băng thông là tài nguyên hữu hạn, cần kiểm tra.

**LEDC**
- Các kênh dùng chung một timer **phải cùng tần số và cùng độ phân giải**.
  Servo 50Hz và LED 5kHz phải nằm trên hai timer khác nhau.
- Ràng buộc: `freq_hz × 2^resolution` không vượt clock nguồn. Tần số cao thì phân giải phải giảm.
- ESP32 classic có high-speed và low-speed mode; các chip mới chỉ có low-speed.

**MCPWM**
- Chỉ ESP32, S3, C6 có. S2 và C3 **không có** — cầu H cần dead-time phải dùng chip khác
  hoặc driver ngoài có dead-time tích hợp.

**TWAI / CAN**
- Chip chỉ có controller, **bắt buộc transceiver ngoài** (SN65HVD230, MCP2551…).
- Bus cần trở đầu cuối 120Ω ở hai đầu, không phải ở mỗi node.
- Kiểm tra transceiver dùng 3.3V hay 5V — bản 5V cần level shifter ở chân RX.

**I2C**
- C3/C6 chỉ có **1 port**. Cần hai bus độc lập (trùng địa chỉ thiết bị) thì phải dùng
  I2C mux (TCA9548A) hoặc đổi chip.
- Thiết bị trùng địa chỉ trên cùng bus là xung đột — kiểm tra danh sách địa chỉ từ datasheet.

**UART**
- UART0 mặc định là console/log. Gán cho thiết bị ngoài thì mất log — phải hỏi người dùng
  có chấp nhận không.
- C3/C6 chỉ 2 UART; dùng hết thì không còn cổng debug.

**Timer / DMA**
- Hai ngoại vi cùng đòi một timer group hoặc kênh DMA là xung đột. Thường gặp khi dùng đồng thời
  I2S, ADC continuous, SPI DMA và RMT.
- ADC continuous mode chiếm DMA — kiểm tra trước khi thêm ngoại vi DMA khác.

## IOMUX và GPIO matrix

ESP32 định tuyến ngoại vi ra chân theo hai đường:

| | IOMUX | GPIO matrix |
|---|---|---|
| Chân | cố định theo từng ngoại vi | gần như chân nào cũng được |
| Tốc độ | cao nhất chip hỗ trợ | bị giới hạn (thường ~40MHz với SPI) |
| Độ trễ / jitter | thấp | cao hơn |

Hệ quả thực tế:
- SPI trên 40MHz, I2S audio/video, tín hiệu cần timing chặt → **phải dùng chân IOMUX mặc định**.
- I2C, UART tốc độ thường, LEDC, GPIO thường → GPIO matrix thoải mái.
- Chân IOMUX mặc định của từng ngoại vi khác nhau theo chip — **tra datasheet, không đoán**.

## Khi hết tài nguyên

Thứ tự cân nhắc, nêu rõ đánh đổi cho người dùng:
1. Gộp thiết bị lên bus dùng chung (nhiều slave trên một SPI/I2C).
2. Dùng mux/expander ngoài (TCA9548A cho I2C, PCF8574/MCP23017 cho GPIO, 74HC4051 cho analog).
3. Thay ngoại vi bằng cách khác (RMT thay cho bit-bang, PCNT thay cho ngắt dày).
4. Đổi sang dòng chip nhiều tài nguyên hơn (C3 → S3).

Lựa chọn 4 ảnh hưởng toàn bộ dự án — chỉ đề xuất, không tự quyết.
