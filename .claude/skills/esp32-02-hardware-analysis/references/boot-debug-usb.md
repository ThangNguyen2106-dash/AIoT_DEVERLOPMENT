# Boot, strapping, debug và USB

Phục vụ hạng mục #2 và #8 của checklist. Đây là nhóm gây "board không boot" và
"không nạp được firmware" nhiều nhất.

## Strapping pin theo chip

Mức logic của các chân này **lúc reset** quyết định chế độ boot. Sau khi boot xong chúng
trở thành GPIO thường.

| Chip | Strapping pin | Ghi chú quan trọng |
|---|---|---|
| ESP32 | GPIO0, 2, 4, 5, 12, 15 | **GPIO12 (MTDI)** kéo cao → đặt điện áp flash 1.8V → module không khởi động |
| ESP32-S2 | GPIO0, 45, 46 | GPIO45 = VDD_SPI (điện áp flash); GPIO46 input-only |
| ESP32-S3 | GPIO0, 3, 45, 46 | GPIO45 = VDD_SPI; GPIO46 có ràng buộc riêng |
| ESP32-C3 | GPIO2, 8, 9 | GPIO9 = boot mode (tương đương nút BOOT) |
| ESP32-C6 | GPIO4, 5, 8, 9, 15 | GPIO9 = boot mode |

GPIO0 (hoặc GPIO9 trên C3/C6) kéo thấp lúc reset → vào **download mode**. Đây là cơ chế nạp
firmware; mạch ngoài giữ chân này thấp sẽ làm thiết bị không bao giờ chạy firmware.

## Quy tắc dùng strapping pin

Tránh hoàn toàn nếu còn chân khác. Buộc phải dùng thì:

- Chỉ dùng làm **OUTPUT**, và mạch ngoài không được kéo ngược mức lúc reset.
- Không nối LED có trở kéo lên nguồn vào strapping pin — lỗi phổ biến làm board không boot.
- Không nối nút nhấn kéo sai chiều so với mức mặc định.
- Thiết bị ngoài kéo mạnh (driver, transceiver có pull cố định) nối vào strapping pin là CHẶN.
- Nếu dùng làm INPUT: phải bảo đảm mức lúc reset luôn đúng, thường bằng pull ngoài mạnh hơn
  nguồn tín hiệu, và ghi rõ ràng buộc này vào tài liệu board.

**Triệu chứng nhận biết**: board chỉ boot khi rút một dây nào đó, boot được khi cấp nguồn nhưng
không boot khi nhấn reset, hoặc log dừng ở `waiting for download`.

## UART0 và console

UART0 mặc định là console log và là cổng nạp firmware qua bootloader.
- Gán UART0 cho thiết bị ngoài → mất log và dữ liệu thiết bị lẫn log lúc boot.
- Thiết bị ngoài gửi dữ liệu vào RX0 lúc boot có thể can thiệp quá trình khởi động.
- Cần dùng UART0 cho ứng dụng thì phải hỏi người dùng có chấp nhận mất đường debug không,
  và đề xuất chuyển log sang UART khác hoặc USB-Serial-JTAG.

## USB

| Chip | Khả năng | Chân |
|---|---|---|
| ESP32 | không có USB — cần chip bridge ngoài (CP2102, CH340) | — |
| ESP32-S2 | USB-OTG | GPIO19/20 |
| ESP32-S3 | USB-OTG **và** USB-Serial-JTAG | GPIO19/20 |
| ESP32-C3 | USB-Serial-JTAG | GPIO18/19 |
| ESP32-C6 | USB-Serial-JTAG | GPIO12/13 |

Quy tắc:
- Chân USB D+/D− **không được gán việc khác** nếu còn dùng USB (nạp firmware, log, JTAG).
- Gán chân USB cho GPIO khác sẽ mất cả đường nạp lẫn đường debug — nếu firmware lỗi,
  có thể phải nạp lại bằng UART hoặc bằng cách giữ nút BOOT thủ công. Đây là mức CẢNH BÁO
  tối thiểu, CHẶN nếu board không có đường nạp dự phòng.
- USB-OTG và USB-Serial-JTAG trên S3 dùng chung chân vật lý — chọn một, không dùng đồng thời.
- Mạch USB cần trở chuỗi và bảo vệ ESD đúng; sai gây lỗi enumerate chập chờn.

## JTAG

| Chip | JTAG |
|---|---|
| ESP32 | MTDI/MTCK/MTMS/MTDO = GPIO12/13/14/15 (chân ngoài) |
| S2/S3/C3/C6 | JTAG qua USB-Serial-JTAG tích hợp, không tốn chân riêng |

Trên ESP32 classic, dùng GPIO12–15 cho việc khác = mất JTAG. GPIO12 còn là strapping pin
điện áp flash — chân này rủi ro kép.

## Nút BOOT / EN và mạch auto-reset

- Phải vào được download mode để nạp firmware. Kiểm tra nút BOOT và EN hoạt động,
  hoặc mạch auto-reset (DTR/RTS qua transistor) đúng.
- Mạch ngoài nối vào EN (reset) phải không giữ chân này ở mức sai.
- Tụ trên chân EN quá lớn làm reset không nhả; quá nhỏ làm reset không sạch khi nguồn lên chậm.
- Board custom **phải có đường nạp firmware dự phòng** (chân UART0 + GPIO0 đưa ra test point),
  kể cả khi thường ngày nạp qua USB. Thiếu đường này là rủi ro sản xuất — nêu ở mức CẢNH BÁO.

## Điện áp flash — rủi ro làm hỏng module

Module dùng flash 1.8V hay 3.3V được quyết định bởi eFuse hoặc strapping (VDD_SPI).
Cấu hình sai điện áp flash có thể làm module không boot hoặc hỏng chip flash.
**Không bao giờ tự đề xuất thay đổi eFuse VDD_SPI** — đây là thao tác một chiều
(xem `esp32-10-security`).
