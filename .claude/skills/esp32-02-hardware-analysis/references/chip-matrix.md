# Ma trận năng lực SoC

Dùng để xác minh ngoại vi dự án cần **có tồn tại trên dòng chip này không**.
Số instance cụ thể và ràng buộc định tuyến xem `peripheral-matrix.md`.

> Bảng này là bộ lọc nhanh ở mức dòng chip. Với module cụ thể (hậu tố N/R) và biến thể
> đóng gói, phải đối chiếu datasheet — xem mục cuối.

## Lõi và bộ nhớ

| | ESP32 | ESP32-S2 | ESP32-S3 | ESP32-C3 | ESP32-C6 |
|---|---|---|---|---|---|
| Lõi | 2× Xtensa LX6 | 1× Xtensa LX7 | 2× Xtensa LX7 | 1× RISC-V | 1× RISC-V + LP core |
| SRAM nội | ~520 KB | ~320 KB | ~512 KB | ~400 KB | ~512 KB |
| PSRAM | tuỳ module (quad) | có | có (quad/octal) | KHÔNG | KHÔNG |
| Flash | ngoài / in-package | ngoài / in-package | ngoài / in-package | ngoài / in-package | ngoài / in-package |

## Vô tuyến

| | ESP32 | ESP32-S2 | ESP32-S3 | ESP32-C3 | ESP32-C6 |
|---|---|---|---|---|---|
| Wi-Fi | 4 (2.4G) | 4 | 4 | 4 | 6 (2.4G) |
| Bluetooth Classic | **CÓ** | không | không | không | không |
| BLE | 4.2 | **KHÔNG** | 5.0 | 5.0 | 5.3 |
| 802.15.4 (Thread/Zigbee) | không | không | không | không | **CÓ** |

Không dòng ESP32 nào hỗ trợ Wi-Fi băng 5 GHz.

## Ngoại vi số

| | ESP32 | ESP32-S2 | ESP32-S3 | ESP32-C3 | ESP32-C6 |
|---|---|---|---|---|---|
| USB | không (cần bridge ngoài) | USB-OTG | USB-OTG + USB-Serial-JTAG | USB-Serial-JTAG | USB-Serial-JTAG |
| TWAI/CAN | có | có | có | có | có |
| Ethernet MAC | có | không | không | không | không |
| SD/MMC host | có | không | có | không | không |
| Touch sensor | 10 kênh | 14 kênh | 14 kênh | **KHÔNG** | **KHÔNG** |
| Temperature sensor | không | có | có | có | có |
| Hall sensor | có (API gỡ từ IDF v5) | không | không | không | không |

## Analog

| | ESP32 | ESP32-S2 | ESP32-S3 | ESP32-C3 | ESP32-C6 |
|---|---|---|---|---|---|
| DAC | 2× 8-bit | 2× 8-bit | **KHÔNG** | **KHÔNG** | **KHÔNG** |
| ADC | 2 unit, 12-bit | 2 unit, 13-bit | 2 unit, 12-bit | 2 unit, 12-bit | 2 unit, 12-bit |
| ADC2 khi Wi-Fi bật | **không dùng được** | hạn chế | hạn chế | hạn chế | hạn chế |

Luôn ưu tiên ADC1 cho cảm biến analog. Không có DAC thì thay bằng LEDC PWM + lọc RC —
nêu rõ đây là thay thế gần đúng, không phải DAC thật.

## Bẫy sinh code sai chip

| Lỗi | Hậu quả |
|---|---|
| Sinh code Bluetooth Classic (SPP, A2DP) cho S2/S3/C3/C6 | không tồn tại, không biên dịch được |
| Sinh code BLE cho S2 | S2 không có Bluetooth |
| `dac_output_voltage()` trên S3/C3/C6 | không có DAC |
| `touch_pad_*` trên C3/C6 | không có touch |
| `hall_sensor_read()` | API đã gỡ khỏi ESP-IDF v5 |
| `xTaskCreatePinnedToCore(..., 1)` trên C3/C6 | chỉ 1 lõi, core 1 không hợp lệ — dùng `tskNO_AFFINITY` |
| Dùng Ethernet MAC nội trên S2/S3/C3/C6 | chỉ ESP32 classic có |

## Phải tra datasheet, KHÔNG suy từ bảng này

Bảng trên nói về **dòng chip**. Những thứ sau phụ thuộc **module/biến thể cụ thể** và
bắt buộc đọc datasheet hoặc nhãn module:

- Dung lượng flash và có PSRAM hay không (hậu tố N/R: N16R8 = 16MB flash + 8MB PSRAM octal).
- PSRAM là quad hay octal — quyết định chân nào bị chiếm.
- Điện áp flash 1.8V hay 3.3V.
- Chân nào thực sự được đưa ra chân module (die có nhiều chân hơn module).
- Ăng-ten tích hợp hay đầu nối ngoài (ảnh hưởng vùng cấm đồng dưới ăng-ten).
- Dải nhiệt độ hoạt động (bản thường và bản H/công nghiệp khác nhau).

Thiếu những thông tin này → hỏi tên module đầy đủ, hoặc yêu cầu datasheet. Không đoán.
