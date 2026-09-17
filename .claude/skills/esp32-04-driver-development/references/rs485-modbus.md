# RS485 và Modbus RTU

## RS485 half-duplex

```c
ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 512, 512, 20, &q, 0));
ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &cfg));
ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, TX_GPIO, RX_GPIO, RTS_GPIO, UART_PIN_NO_CHANGE));
ESP_ERROR_CHECK(uart_set_mode(UART_NUM_1, UART_MODE_RS485_HALF_DUPLEX));
```

- Chân RTS lái DE/RE của transceiver (MAX485, SP3485...). Để IDF tự lái. Tự bật/tắt bằng GPIO
  gần như luôn cắt mất byte cuối, vì `uart_write_bytes` trả về khi dữ liệu vào FIFO chứ chưa
  phát xong. Nếu buộc phải tự lái thì `uart_wait_tx_done()` rồi mới hạ DE.
- Một số mạch có echo: byte vừa gửi quay lại RX. Phải nhận diện và bỏ, đừng parse nhầm thành trả lời.
- Điện trở kết cuối 120 Ω chỉ đặt ở **hai đầu** đường dây, không đặt ở mọi node.
- Phải có GND chung (hoặc transceiver cách ly). Thiếu GND thì chạy tốt trên bàn, lỗi ngẫu nhiên
  ngoài thực địa — triệu chứng rất giống bug phần mềm.
- Dây dài thì hạ baud: 9600 đi được vài trăm mét, 115200 thì không.

## Modbus RTU — dùng thư viện, đừng tự viết

ESP-IDF có `esp-modbus`: `idf.py add-dependency "espressif/esp-modbus"`. Chỉ tự viết stack khi
thiết bị đối tác lệch chuẩn tới mức thư viện không tải được.

```c
mb_communication_info_t comm = {
    .port = UART_NUM_1, .mode = MB_MODE_RTU,
    .baudrate = 9600, .parity = MB_PARITY_NONE,
};
void *master = NULL;
ESP_ERROR_CHECK(mbc_master_init(MB_PORT_SERIAL_MASTER, &master));
ESP_ERROR_CHECK(mbc_master_setup(&comm));
ESP_ERROR_CHECK(mbc_master_start());
ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, TX, RX, RTS, UART_PIN_NO_CHANGE));
ESP_ERROR_CHECK(uart_set_mode(UART_NUM_1, UART_MODE_RS485_HALF_DUPLEX));
ESP_ERROR_CHECK(mbc_master_set_descriptor(device_params, num_params));
```

Thứ tự quan trọng: `uart_set_pin` và `uart_set_mode` gọi **sau** `mbc_master_start()`,
nếu không cấu hình bị ghi đè.

## Bẫy Modbus hay gặp

| Triệu chứng | Nguyên nhân |
|---|---|
| Timeout ở mọi thanh ghi | sai slave ID, sai baud/parity, đảo dây A/B |
| Đọc được nhưng giá trị vô nghĩa | lệch offset 1 (40001 ↔ offset 0), sai kiểu dữ liệu |
| Số 32-bit sai | word order ngược giữa hai thanh ghi — thử hoán đổi |
| Thỉnh thoảng lỗi CRC | thiếu kết cuối, thiếu GND chung, nhiễu từ biến tần |
| Slave trả chậm | timeout mặc định quá ngắn; thiết bị công nghiệp thường cần 300–1000 ms |

## Quy ước driver Modbus

- **Một task duy nhất sở hữu bus.** Task khác gửi yêu cầu qua queue, nhận kết quả qua queue
  hoặc callback. Không bao giờ hai task cùng gọi vào stack Modbus.
- Đọc gộp dải thanh ghi liền nhau trong một lệnh, thay vì đọc từng thanh ghi.
- Giữa hai frame phải có khoảng nghỉ ≥ 3.5 ký tự. Thư viện lo việc này; tự viết thì phải tính.
- Lỗi một thiết bị không được chặn vòng quét: đánh dấu thiết bị đó lỗi, sang thiết bị kế,
  thử lại ở vòng sau với backoff.
- Bảng thanh ghi (địa chỉ, kiểu, hệ số scale, đơn vị) là **dữ liệu cấu hình** — để trong một bảng
  `static const`, không rải trong code, không lẫn vào logic đọc.
