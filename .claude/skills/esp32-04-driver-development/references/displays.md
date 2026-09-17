# Màn hình

## Chọn cách làm theo loại màn

| Loại | Giao tiếp | Cách nên dùng |
|---|---|---|
| LCD ký tự 16x2 / 20x4 (HD44780 + PCF8574) | I2C | driver tự viết, rất đơn giản |
| OLED SSD1306 / SH1106 | I2C, SPI | component có sẵn (`esp_lcd_panel_ssd1306`) hoặc tự viết |
| TFT ST7789 / ILI9341 | SPI | `esp_lcd` (`esp_lcd_panel_io_spi` + panel driver) |
| RGB / MIPI-DSI (S3, P4) | song song | `esp_lcd` + framebuffer PSRAM |
| E-paper | SPI | component nhà sản xuất; chú ý chu kỳ refresh |

Không tự viết lại driver panel khi `esp_lcd` đã hỗ trợ — sai timing SPI ở TFT rất khó chẩn đoán.

## Nguyên tắc driver màn hình

- Driver chỉ biết **vẽ**: pixel, buffer, text. Không biết "màn hình trạng thái" hay "trang cài đặt"
  — đó là UI, thuộc `esp32-06-application-development`.
- API tối thiểu: `create / clear / draw_bitmap / flush / set_backlight / destroy`.
- Cập nhật màn hình không được chặn vòng điều khiển. Gửi nội dung qua queue cho một task
  chuyên trách màn hình; task đo/điều khiển không tự vẽ.
- Chỉ vẽ lại vùng thay đổi. Vẽ toàn màn hình mỗi chu kỳ là nguyên nhân phổ biến nhất khiến
  thiết bị giật và trễ.

## esp_lcd với TFT SPI

```c
esp_lcd_panel_io_spi_config_t io_cfg = {
    .dc_gpio_num = DC, .cs_gpio_num = CS,
    .pclk_hz = 40 * 1000 * 1000,
    .lcd_cmd_bits = 8, .lcd_param_bits = 8,
    .spi_mode = 0, .trans_queue_depth = 10,
};
esp_lcd_panel_io_handle_t io;
ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_cfg, &io));

esp_lcd_panel_dev_config_t panel_cfg = { .reset_gpio_num = RST, .bits_per_pixel = 16 };
esp_lcd_panel_handle_t panel;
ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &panel_cfg, &panel));
ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
```

- Buffer vẽ phải cấp bằng `heap_caps_malloc(n, MALLOC_CAP_DMA)`. Buffer trên stack không hợp lệ
  cho DMA và là nguyên nhân kinh điển của "màn hiện rác".
- Màu đảo/ngược, lệch vài pixel, gương trái phải: chỉnh bằng `esp_lcd_panel_invert_color`,
  `_mirror`, `_swap_xy`, `_set_gap` — không tự dịch dữ liệu trong buffer.
- Đèn nền là chân riêng, lái qua LEDC nếu cần chỉnh độ sáng → `pwm-motor.md`.

## Triệu chứng thường gặp

| Hiện tượng | Nguyên nhân |
|---|---|
| Trắng trơn / đen trơn | chưa reset đúng, sai chân DC hoặc RST, chưa `disp_on` |
| Nhiễu sọc, rác ngẫu nhiên | pclk quá cao cho dây nối, buffer không phải DMA |
| Màu đảo (đỏ ↔ xanh) | thứ tự RGB/BGR, chưa `invert_color` |
| Lệch vài pixel ở mép | sai `set_gap` cho biến thể panel đó |
| Treo khi vẽ | chờ transaction SPI không timeout, hoặc vẽ từ ISR |

## LVGL

Chỉ thêm LVGL khi thật sự cần UI phức tạp (nhiều trang, widget, cảm ứng). Nó ăn RAM đáng kể
và kéo theo ràng buộc kiến trúc: LVGL **không thread-safe**, mọi lời gọi `lv_*` phải ở cùng một
task, hoặc bọc bằng mutex. Hiển thị vài dòng số liệu thì vẽ trực tiếp là đủ.
