# UART

```c
uart_config_t cfg = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
};
QueueHandle_t uart_queue;
ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 2048, 0, 20, &uart_queue, 0));
ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &cfg));
ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, TX_GPIO, RX_GPIO,
                             UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
```

## Lưu ý

- UART0 thường là console/log. Dùng cho thiết bị ngoài sẽ lẫn với log — dùng UART1/UART2.
- Đọc theo **event queue** (`UART_DATA`, `UART_FIFO_OVF`, `UART_BUFFER_FULL`) thay vì
  polling `uart_read_bytes` trong vòng lặp bận.
- Giao thức có ký tự kết thúc: dùng `uart_enable_pattern_det_baud_intr`.
- Buffer RX phải đủ cho burst dữ liệu. Tràn buffer biểu hiện là mất byte giữa chừng,
  rất dễ bị chẩn đoán nhầm thành sai baud rate.
- RS485 half-duplex: `uart_set_mode(UART_NUM_x, UART_MODE_RS485_HALF_DUPLEX)` để IDF
  tự lái chân DE/RE. Tự điều khiển bằng GPIO dễ cắt mất byte cuối.
- Parser giao thức phải chịu được dữ liệu đến từng phần. Viết dạng máy trạng thái;
  không giả định mỗi lần đọc là một frame trọn vẹn.
- Có checksum/CRC trong giao thức thì luôn kiểm tra, và bỏ frame hỏng thay vì cố suy đoán.
