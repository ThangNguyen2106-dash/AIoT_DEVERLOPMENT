# SPI

```c
spi_bus_config_t buscfg = {
    .mosi_io_num = 11, .miso_io_num = 13, .sclk_io_num = 12,
    .quadwp_io_num = -1, .quadhd_io_num = -1,
    .max_transfer_sz = 4092,
};
ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

spi_device_interface_config_t devcfg = {
    .clock_speed_hz = 10 * 1000 * 1000,
    .mode = 0,                 /* CPOL/CPHA phải khớp datasheet, sai là đọc ra rác */
    .spics_io_num = 10,
    .queue_size = 4,
};
spi_device_handle_t dev;
ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &dev));
```

## Lưu ý

- `SPI1_HOST` gắn với flash nội bộ — KHÔNG dùng cho thiết bị ngoài.
- Buffer dùng DMA nên cấp bằng `heap_caps_malloc(n, MALLOC_CAP_DMA)`, căn 4 byte.
  Buffer nằm trên stack hoặc trên PSRAM có thể không hợp lệ cho DMA.
- Chân SPI mặc định (IOMUX) cho tốc độ cao nhất; chân khác đi qua GPIO matrix và bị giới hạn.
- Nhiều slave khác tốc độ/mode: thêm nhiều `spi_device` trên cùng bus, IDF tự đổi cấu hình
  theo từng device. Không tự viết code đổi tốc độ thủ công.
- Truyền lớn (LCD, thẻ SD): dùng `spi_device_queue_trans` + `spi_device_get_trans_result`
  để chồng lấn DMA với xử lý, thay vì `spi_device_transmit` blocking.
- CS thủ công (`spics_io_num = -1` rồi tự điều khiển GPIO) chỉ dùng khi thiết bị đòi giữ CS
  qua nhiều transaction; khi đó phải tự bảo đảm không task nào chen vào giữa.
