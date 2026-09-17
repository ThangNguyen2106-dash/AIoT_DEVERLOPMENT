# Bọc cảm biến thành component

## Cấu trúc component

```
components/sht3x/
├── CMakeLists.txt        # idf_component_register(SRCS "sht3x.c" INCLUDE_DIRS "include" REQUIRES driver)
├── include/sht3x.h       # API công khai + comment hợp đồng
├── sht3x.c
└── README.md             # nối dây, ví dụ dùng, dải đo, sai số
```

Trong `sht3x.c` tách ba nhóm hàm, đừng trộn:

1. **I/O** — chỉ gọi bus, trả `esp_err_t`, không tính toán.
2. **Chuyển đổi** — hàm thuần `raw → đơn vị kỹ thuật`, không tham số handle, test được trên host.
3. **Kiểm tra** — CRC và dải hợp lệ, cũng là hàm thuần.

```c
float sht3x_raw_to_celsius(uint16_t raw) { return -45.0f + 175.0f * raw / 65535.0f; }
bool  sht3x_crc_ok(const uint8_t *d, uint8_t crc);
```

## Init phải xác minh thiết bị có thật

Đọc thanh ghi ID / thanh ghi hằng số và so với giá trị datasheet. Không làm bước này thì
`create()` báo thành công cả khi chưa cắm dây, và lỗi chỉ lộ ra sau đó rất xa.

```c
uint8_t id = 0;
ESP_RETURN_ON_ERROR(read_reg(dev, REG_CHIP_ID, &id, 1), TAG, "read id");
if (id != CHIP_ID_EXPECTED) {
    ESP_LOGE(TAG, "chip id 0x%02x, expected 0x%02x", id, CHIP_ID_EXPECTED);
    return ESP_ERR_NOT_FOUND;
}
```

Nhớ tôn trọng thời gian khởi động sau cấp nguồn ghi trong datasheet (thường 1–100 ms).

## Thời gian chuyển đổi

Cảm biến chậm (DS18B20 750 ms, BME280 ở oversampling cao, MAX31855) thì **tách start/read**
thay vì chặn task — xem `driver-contract.md` mục 2. Cảm biến nhanh (dưới ~20 ms) có thể chờ
trong hàm đọc, nhưng phải ghi rõ thời gian chặn tối đa trong header.

## Kiểm tra dữ liệu

| Kiểm tra | Cách làm |
|---|---|
| CRC/checksum | luôn kiểm nếu giao thức có; sai thì trả `ESP_ERR_INVALID_CRC`, không đoán |
| Dải vật lý | nhiệt độ −40..85 °C, độ ẩm 0..100 %, áp suất 300..1100 hPa... ngoài dải là lỗi |
| Giá trị chết | toàn `0x00` hoặc toàn `0xFF` gần như luôn là lỗi bus, không phải số đo |
| Nhảy bậc | biến thiên vật lý có giới hạn; nhảy quá lớn giữa hai mẫu liền kề là đáng ngờ |

Bộ lọc nhiễu: trung vị 5–9 mẫu loại gai tốt hơn trung bình; tín hiệu chậm thì thêm IIR bậc một.
Lọc là **xử lý tín hiệu**, đặt trong driver được, nhưng phải cho tắt được và không được giấu
mất thông tin lỗi (mẫu hỏng thì bỏ, không đưa vào bộ lọc).

## Hiệu chuẩn

- Hệ số hiệu chuẩn (offset, gain, đường cong) là **dữ liệu**, không phải hằng số biên dịch.
  Nhận qua config hoặc đọc từ NVS; driver chỉ áp dụng.
- Quy trình hiệu chuẩn, lưu trữ hệ số → `esp32-06-application-development`.
- Cảm biến có dữ liệu hiệu chuẩn nhà máy trong chip (BME280, SHT, MAX3185x): đọc đúng một lần
  lúc `create()`, lưu trong handle, không đọc lại mỗi lần đo.

## Nhiều cảm biến cùng loại

Handle-based là đủ: `sht3x_create()` hai lần với địa chỉ khác nhau. Không tạo mảng static
trong driver, không đánh số instance, không dùng singleton.

## Cảm biến 1-Wire (DS18B20)

ESP-IDF không có driver 1-Wire lõi; dùng component `espressif/onewire_bus` (qua RMT) thay vì
tự bit-bang bằng `esp_rom_delay_us` — bit-bang thủ công rất dễ sai timing khi bị ngắt chen ngang.
