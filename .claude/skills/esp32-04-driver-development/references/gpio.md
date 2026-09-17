# GPIO

```c
gpio_config_t io = {
    .pin_bit_mask = (1ULL << RELAY_GPIO) | (1ULL << LED_GPIO),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
};
ESP_ERROR_CHECK(gpio_config(&io));
gpio_set_level(RELAY_GPIO, 0);      /* đặt mức AN TOÀN ngay sau khi cấu hình */
```

## Bẫy

- Giữa lúc reset và lúc `gpio_config` chạy, chân ở trạng thái float. Chân điều khiển tải phải có
  pull-down/pull-up **ngoài** định trạng thái an toàn — đây là vấn đề phần cứng, nhắc user,
  không sửa được bằng phần mềm.
- Chân chỉ vào (ESP32: GPIO34–39) không có pull-up/pull-down nội bộ và không xuất ra được.
- Strapping pin (GPIO0, 2, 12, 15 trên ESP32) kéo sai mức lúc boot thì không khởi động được
  → `esp32-02-hardware-analysis`.
- Chân nối flash/PSRAM (ESP32: 6–11; S3: tuỳ module) không được dùng.
- Dòng ra mỗi chân khoảng 20 mA. Relay, còi, motor phải qua transistor/MOSFET/driver.
- ESP32 là 3.3 V, không chịu 5 V. Thiết bị 5 V cần level shifter.

## Ngắt ngoài

```c
ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM));
ESP_ERROR_CHECK(gpio_isr_handler_add(BTN_GPIO, btn_isr, (void *)BTN_GPIO));

static void IRAM_ATTR btn_isr(void *arg)
{
    BaseType_t hp = pdFALSE;
    uint32_t gpio = (uint32_t)arg;
    xQueueSendFromISR(evt_queue, &gpio, &hp);   /* chỉ đẩy sự kiện */
    if (hp) portYIELD_FROM_ISR();
}
```

- ISR phải `IRAM_ATTR`, không `ESP_LOGx`, không `malloc`, không hàm blocking.
- Xử lý nặng làm ở task nhận queue.
- `gpio_install_isr_service` gọi đúng một lần cho cả chương trình. Nhiều driver cùng cần thì gọi
  ở tầng khởi tạo chung, và chấp nhận `ESP_ERR_INVALID_STATE` khi đã cài.

## Chống dội (debounce)

Nút cơ dội 5–20 ms. Ba cách, chọn theo nhu cầu:

1. **Lọc theo thời gian trong task** — ISR gửi sự kiện, task bỏ qua sự kiện cách sự kiện trước
   dưới 30 ms (dùng `esp_timer_get_time()`, không dùng tick). Đủ cho hầu hết trường hợp.
2. **Lấy mẫu định kỳ** — task đọc chân mỗi 10 ms, chốt trạng thái khi 3 lần liên tiếp giống nhau.
   Không cần ngắt, rất ổn định, trễ có giới hạn biết trước.
3. **Phần cứng** — tụ RC hoặc Schmitt trigger, khi nhiễu nặng hoặc dây dài.

Không debounce bằng `vTaskDelay` trong ISR hay busy-wait trong callback.

## Open-drain, giữ mức khi ngủ

- `GPIO_MODE_OUTPUT_OD` cho bus một dây (1-Wire, ngắt chung nhiều thiết bị): mọi thiết bị chỉ kéo
  xuống, pull-up ngoài kéo lên. Bắt buộc pull-up ngoài (4.7 kΩ điển hình).
- `gpio_hold_en()` + `gpio_deep_sleep_hold_en()` giữ mức chân qua deep sleep
  → `esp32-11-power-management`.
