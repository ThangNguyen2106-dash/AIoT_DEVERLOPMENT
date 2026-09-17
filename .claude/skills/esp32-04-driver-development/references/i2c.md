# I2C trên ESP-IDF v5

API cũ `i2c_master_write_to_device()` (`driver/i2c.h`) đã deprecated từ v5.2.
Dùng `driver/i2c_master.h`:

```c
i2c_master_bus_config_t bus_cfg = {
    .i2c_port = I2C_NUM_0,
    .sda_io_num = BOARD_I2C_SDA_GPIO,
    .scl_io_num = BOARD_I2C_SCL_GPIO,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,   /* chỉ đủ cho 100kHz dây ngắn khi test */
};
i2c_master_bus_handle_t bus;
ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = 0x76,
    .scl_speed_hz = 400000,
};
i2c_master_dev_handle_t dev;
ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &dev));

uint8_t reg = 0xD0, id = 0;
esp_err_t err = i2c_master_transmit_receive(dev, &reg, 1, &id, 1, pdMS_TO_TICKS(100));
```

## Chẩn đoán lỗi

| Triệu chứng | Nguyên nhân thường gặp |
|---|---|
| `ESP_ERR_TIMEOUT` / `ESP_FAIL` ở mọi lệnh | thiếu pull-up, sai chân SDA/SCL, thiết bị chưa có nguồn |
| Đọc ra toàn 0xFF hoặc toàn 0x00 | không có ACK, sai địa chỉ (nhầm 8-bit và 7-bit) |
| 100 kHz chạy ổn, 400 kHz lỗi | pull-up quá yếu, dây quá dài, điện dung bus lớn |
| Bus treo sau khi reset giữa transaction | slave đang giữ SDA thấp — cần phát 9 xung clock để giải phóng |

Địa chỉ trong datasheet thường ghi dạng 8-bit (đã gồm bit R/W). ESP-IDF cần 7-bit, dịch phải 1 bit.

Không rõ địa chỉ thì quét bus: lặp `i2c_master_probe(bus, addr, 50)` từ 0x08 đến 0x77.

## Nhiều thiết bị trên một bus
- Trùng địa chỉ thì phải dùng I2C mux (TCA9548A) hoặc bus thứ hai.
- Truy cập từ nhiều task: bọc bằng mutex, hoặc gom mọi truy cập bus về một task duy nhất
  nhận lệnh qua queue. Driver IDF v5 có khoá nội bộ nhưng transaction nhiều bước
  (ghi thanh ghi rồi đọc) vẫn có thể bị chen ngang nếu không tự khoá.
