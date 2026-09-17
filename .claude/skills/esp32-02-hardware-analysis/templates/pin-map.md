# Pin map — <tên dự án>

Tài liệu nguồn cho `board_config.h`. Cập nhật ở đây trước, rồi mới sửa code.

## Cấu hình

| | |
|---|---|
| Module | ESP32-S3-WROOM-1-N16R8 |
| Board | <tên + revision, hoặc "custom rev A"> |
| Flash / PSRAM | 16 MB / 8 MB octal |
| Nguồn bằng chứng | schematic rev A (hạng 1) + datasheet module (hạng 2) |
| Ngày thẩm định | YYYY-MM-DD |

## Bảng phân bổ chân

| Ngoại vi | Tín hiệu | GPIO | Hướng | Pull | IOMUX | Ghi chú | Trạng thái |
|---|---|---|---|---|---|---|---|
| I2C0 | SDA | 8 | OD | ngoài 4.7k | — | BME280 0x76, OLED 0x3C | OK |
| I2C0 | SCL | 9 | OD | ngoài 4.7k | — | 400 kHz | OK |
| SPI2 | SCLK | 12 | OUT | — | có | LCD, 40 MHz | OK |
| SPI2 | MOSI | 11 | OUT | — | có | | OK |
| SPI2 | CS_LCD | 10 | OUT | lên 10k | — | | OK |
| UART1 | TX | 17 | OUT | — | — | modem, 115200 | OK |
| ADC1 | CH0 | 1 | IN | — | — | đo pin qua chia áp 1:2 | OK |
| LEDC | PWM_FAN | 5 | OUT | — | — | 25 kHz, timer 0 | OK |
| GPIO | RELAY | 4 | OUT | **xuống 10k ngoài** | — | an toàn lúc boot | OK |
| GPIO | BTN | 0 | IN | lên (nội) | — | strapping — chỉ đọc sau boot | CẢNH BÁO |

Ký hiệu: OD = open-drain · IOMUX "có" = dùng chân mặc định cho tốc độ cao.

## Chân đã bị chiếm — không được cấp phát

| GPIO | Chiếm bởi |
|---|---|
| 19, 20 | USB-Serial-JTAG (nạp firmware + log) |
| 33–37 | PSRAM octal |
| 43, 44 | UART0 console |
| 0 | strapping / nút BOOT |

## Tài nguyên còn lại

| Ngoại vi | Đã dùng | Còn |
|---|---|---|
| I2C | 1 / 2 | 1 |
| SPI | 1 / 2 | 1 |
| UART | 1 + console / 3 | 1 |
| LEDC kênh | 1 / 8 | 7 |
| LEDC timer | 1 / 4 | 3 |

## Rủi ro đã ghi nhận

| # | Mức | Mô tả | Xử lý |
|---|---|---|---|
| 1 | CHẶN | — | — |
| 2 | CẢNH BÁO | BTN trên GPIO0 (strapping) | chỉ đọc sau khi boot xong; mạch không kéo thấp lúc reset |
| 3 | THÔNG TIN | Còn 1 SPI host | dự phòng cho thẻ SD |

## Câu hỏi còn treo

| # | Câu hỏi | Cần tài liệu gì | Ngày hỏi | Trạng thái |
|---|---|---|---|---|
| 1 | Relay module dùng mức kích 3.3V hay 5V? | datasheet, mục Electrical | YYYY-MM-DD | chờ |

**Không thay đổi pin map khi còn câu hỏi ở trạng thái "chờ" liên quan tới chân đó.**

## Lịch sử thay đổi

| Ngày | Thay đổi | Lý do | Người duyệt |
|---|---|---|---|
| YYYY-MM-DD | bản đầu | | |
