# Timeout giao tiếp — ngoại vi và mạng

Phục vụ lớp lỗi: `ESP_ERR_TIMEOUT`, thiết bị không phản hồi, mất kết nối không phục hồi.

## Khoanh vùng trước khi sửa

Timeout chỉ nói "không có phản hồi trong thời hạn". Nó **không** nói lỗi ở đâu. Bốn khả năng:

| Vùng | Nghĩa | Phép thử tách |
|---|---|---|
| Cấu hình | firmware cấu hình sai chân/tốc độ/địa chỉ | đọc lại cấu hình, so với pin map |
| Điện | dây, pull-up, mức logic, nguồn, nhiễu | oscilloscope / logic analyzer; đổi dây; giảm tốc |
| Thiết bị đối tác | cảm biến/slave/server lỗi hoặc bận | thử thiết bị khác; thử master khác |
| Phần mềm ta | thời điểm gọi, thiếu delay khởi động, tranh chấp bus | log timestamp; chạy tuần tự hoá bus |

Quy tắc phân biệt quan trọng nhất:

| Quan sát | Kết luận |
|---|---|
| **Lỗi ngay từ lần đầu, mọi lần** | cấu hình hoặc đấu nối — không phải lỗi chập chờn |
| **Chạy tốt rồi lỗi sau một thời gian** | nhiệt, nguồn, rò tài nguyên, hoặc thiết bị treo |
| **Chỉ lỗi khi có tải/động cơ chạy** | nhiễu điện hoặc sụt nguồn |
| **Mọi thiết bị trên bus cùng lỗi** | bus (pull-up, GND, nguồn) |
| **Một thiết bị lỗi, thiết bị khác ok** | thiết bị đó hoặc địa chỉ của nó |

---

## I2C

| Triệu chứng | Nguyên nhân thường gặp |
|---|---|
| `ESP_ERR_TIMEOUT` mọi lệnh, quét bus không thấy gì | thiếu pull-up ngoài, sai chân SDA/SCL, thiếu GND chung |
| `ESP_FAIL` (NACK địa chỉ) | sai địa chỉ 7-bit (nhầm với địa chỉ đã dịch bit), thiết bị chưa cấp nguồn |
| Chạy ở 100kHz, lỗi ở 400kHz | pull-up quá yếu / điện dung bus lớn / dây dài |
| Bus kẹt ở mức thấp, timeout vĩnh viễn | slave đang giữ SDA (clock stretching hoặc slave treo) |
| Thỉnh thoảng lỗi khi motor chạy | nhiễu — cần định tuyến lại dây, chống nhiễu, GND tốt |

Điều tra: quét bus (`i2c` scan) → nếu không thấy địa chỉ nào thì **không phải lỗi driver**, là
điện hoặc chân. Đo mức nghỉ của SDA/SCL: phải ở 3.3V; ở mức thấp nghĩa là bus bị giữ.

Ràng buộc pull-up và mức logic: `esp32-02-hardware-analysis/references/pin-constraints.md`.
Viết/sửa driver I2C: `esp32-04-driver-development/references/i2c.md`.

**Không** sửa bằng cách tăng timeout. Timeout lớn hơn chỉ làm task chờ lâu hơn rồi vẫn thất bại —
và có thể kéo theo TWDT.

---

## SPI

| Triệu chứng | Nguyên nhân |
|---|---|
| Đọc ra toàn `0x00` hoặc `0xFF` | sai MISO/MOSI, CS không được điều khiển, thiết bị chưa nguồn |
| Dữ liệu lệch bit | sai SPI mode (CPOL/CPHA) |
| Chạy chậm thì đúng, nhanh thì sai | dây dài, không dùng chân IOMUX, thiếu kết thúc đường truyền |
| Lỗi khi có thiết bị thứ hai trên bus | CS chồng lấn, hoặc thiết bị không thả MISO |

SPI hiếm khi "timeout" — nó thường trả về dữ liệu **sai** thay vì báo lỗi. Luôn có bước đọc thanh
ghi ID/WHO_AM_I để xác nhận liên kết trước khi tin dữ liệu.

---

## UART / RS485 / Modbus RTU

| Triệu chứng | Nguyên nhân |
|---|---|
| Không nhận được gì | sai baud, sai chân TX/RX (hoán vị), thiếu GND chung |
| Ký tự rác | sai baud, sai khung (parity/stop), crystal sai |
| Mất byte khi dữ liệu nhiều | buffer nhỏ, task đọc quá chậm, không dùng ngắt/DMA |
| RS485: không có phản hồi | chân DE/RE không chuyển đúng lúc, sai phân cực A/B, thiếu terminator |
| Modbus: CRC error rời rạc | nhiễu, hoặc DE nhả quá sớm cắt mất byte cuối |
| Modbus: timeout với một slave | sai slave ID, slave khác baud, slave treo |

RS485 half-duplex: thời điểm nhả DE là lỗi kinh điển — nhả trước khi byte cuối ra khỏi thanh ghi
dịch thì slave nhận thiếu. Dùng `uart_wait_tx_done()` trước khi nhả.

Chi tiết driver: `esp32-04-driver-development/references/uart.md`.

---

## Mạng — Wi-Fi, MQTT, HTTP, TLS

Ở lớp mạng, phân biệt hai chuyện hoàn toàn khác nhau:

| | Ý nghĩa | Thuộc về |
|---|---|---|
| **Mất kết nối** | bình thường, phải xảy ra, phải phục hồi được | thiết kế → `esp32-05-connectivity` |
| **Không phục hồi sau khi mất** | lỗi thật cần điều tra | skill này |

Mất Wi-Fi là sự kiện bình thường ngoài thực địa. Firmware không tự phục hồi mới là bug.

### Bằng chứng cần
```
CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y      # bật tạm để thấy log wifi:/esp-tls/mqtt_client
```
- Disconnect reason code trong `WIFI_EVENT_STA_DISCONNECTED` (`wifi_err_reason_t`) — code nói rõ
  sai mật khẩu, không tìm thấy AP, AP đuổi, hay hết hạn xác thực.
- RSSI khi đang chạy (`esp_wifi_sta_get_ap_info`); dưới khoảng −80 dBm là vùng mất ổn định.
- Log broker/server phía đối tác: kết nối có tới nơi không, bị từ chối vì lý do gì.

### Nguyên nhân thường gặp

| Triệu chứng | Nguyên nhân |
|---|---|
| Mất Wi-Fi rồi không bao giờ kết nối lại | thiếu state machine reconnect, hoặc reconnect bị chặn ở nhánh lỗi |
| Reconnect liên tục, mỗi vài giây | không có backoff; AP quá tải; nguồn yếu khi TX |
| Free heap giảm mỗi lần reconnect | rò tài nguyên trong vòng reconnect → `memory-faults.md` |
| MQTT connect timeout | DNS thất bại, cổng bị chặn, broker từ chối credential |
| TLS handshake thất bại | CA sai/hết hạn, thời gian hệ thống chưa đồng bộ (SNTP), stack task không đủ |
| TLS lỗi rải rác khi dữ liệu lớn | stack không đủ cho mbedTLS, hoặc heap phân mảnh |
| HTTP timeout chỉ với payload lớn | buffer nhỏ, hoặc task bị TWDT chen ngang |
| Kết nối đứng: không lỗi, cũng không dữ liệu | thiếu keepalive/ping → cần phát hiện kết nối chết |

Kết nối TCP "chết im" (half-open) không tự báo lỗi. Không có keepalive ở lớp ứng dụng thì thiết bị
ngồi chờ mãi trên một socket đã chết — đây là bug hay bị hiểu nhầm thành "mất mạng".

Chứng chỉ hết hạn và lệch thời gian là nguyên nhân TLS thất bại phổ biến nhất trên thiết bị nhúng:
chip boot với thời gian 1970, xác thực chứng chỉ sẽ fail cho tới khi SNTP xong. Kiểm tra thứ tự
khởi động trước khi nghi ngờ chứng chỉ.

### Minimal Fix
Bản sửa ở đây thường là **thiết kế lại vòng đời kết nối** — không thuộc skill này.
Xác định nguyên nhân gốc xong thì chuyển: `esp32-05-connectivity`.

---

## Khi timeout kéo theo watchdog

Lời gọi chặn dài (I2C timeout 1s, TLS handshake, HTTP request) chạy trong task đang bị TWDT theo
dõi sẽ làm watchdog kêu. Khi đó có **hai** lỗi: timeout gốc, và task chặn quá lâu. Ghi nhận cả hai;
sửa timeout gốc trước, rồi xét lại việc task chặn (`rtos-faults.md`).
