# CAN / TWAI

ESP32 gọi CAN là TWAI. Chip chỉ có bộ điều khiển, **bắt buộc** có transceiver ngoài
(SN65HVD230, TJA1050, MCP2551 — chú ý mức 3.3 V).

```c
twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(TX_GPIO, RX_GPIO, TWAI_MODE_NORMAL);
g.rx_queue_len = 32;
g.tx_queue_len = 16;
twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

ESP_ERROR_CHECK(twai_driver_install(&g, &t, &f));
ESP_ERROR_CHECK(twai_start());

twai_message_t msg = { .identifier = 0x123, .data_length_code = 8 };
esp_err_t err = twai_transmit(&msg, pdMS_TO_TICKS(100));   /* luôn có timeout */
err = twai_receive(&msg, pdMS_TO_TICKS(1000));
```

## Bắt buộc

- **Bit rate phải giống hệt mọi node.** Lệch thì không node nào nhận được gì.
- Kết cuối 120 Ω ở hai đầu bus. Đo giữa CAN_H và CAN_L khi tắt nguồn phải ra khoảng 60 Ω;
  ra 120 Ω là thiếu một đầu, hở mạch là thiếu cả hai.
- ESP32 gốc có lỗi phần cứng khi là node duy nhất đang hoạt động. Test cần ít nhất hai node thật,
  hoặc dùng `TWAI_MODE_NO_ACK` / self-test mode để thử phát.
- Không ai ACK thì frame bị phát lại liên tục cho tới khi bộ đếm lỗi đẩy node vào **bus-off**.

## Bus-off và recovery — phần hay bị bỏ quên

```c
twai_status_info_t st;
twai_get_status_info(&st);
if (st.state == TWAI_STATE_BUS_OFF) {
    ESP_LOGE(TAG, "bus-off tx_err=%u", (unsigned)st.tx_error_counter);
    twai_initiate_recovery();      /* xong khi bus rảnh đủ 128 lần 11 bit */
    /* chờ state về TWAI_STATE_STOPPED rồi twai_start() lại */
}
```

- Một task giám sát đọc `twai_get_status_info` mỗi giây là đủ, không cần polling nhanh.
- `tx_error_counter` / `rx_error_counter` tăng dần nghĩa là dây, kết cuối hoặc bit rate có vấn đề
  — không phải lỗi code. Đưa hai số này vào `*_get_stats()` để chẩn đoán từ xa.
- `twai_transmit` trả `ESP_ERR_TIMEOUT` (hàng đợi TX đầy) thường nghĩa là không ai nhận:
  báo lên tầng trên, đừng lặng lẽ bỏ frame.
- `twai_receive` là hàm chặn: giao cho một task chuyên trách nhận rồi đẩy vào queue ứng dụng;
  task khác không gọi trực tiếp.

## Bộ lọc

Chỉ quan tâm một số ID thì dùng filter phần cứng thay vì lọc bằng phần mềm — giảm ngắt và tải CPU.
Mask theo bit: bit `0` trong mask nghĩa là bit đó bắt buộc khớp.

## Giao thức tầng trên

J1939, OBD-II, CANopen là giao thức **ứng dụng**. Driver chỉ gửi/nhận frame thô; đóng gói và
diễn giải PGN/SDO/PDO thuộc `esp32-06-application-development`.
