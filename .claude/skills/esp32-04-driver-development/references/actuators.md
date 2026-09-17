# Cơ cấu chấp hành (relay, solenoid, van, stepper, motor driver)

Sai sót ở đây làm hỏng phần cứng hoặc gây nguy hiểm, không chỉ sai dữ liệu. Phần tạo xung PWM
xem `pwm-motor.md`; file này nói về hợp đồng driver và an toàn.

> **File này là chủ sở hữu duy nhất của quy tắc "trạng thái an toàn của cơ cấu chấp hành".**
> Các skill khác chỉ nhắc và trỏ về đây, không định nghĩa lại: `esp32-06` (fail-safe nghiệp vụ),
> `esp32-11` (khoá chống ngủ), `esp32-12` (rà soát trước ship), `esp32-firmware` (pre-flash).
> Quy tắc đổi thì đổi ở đây.

## Bốn quy tắc không được bỏ

1. **Trạng thái an toàn trước.** `create()` đưa chân về mức DỪNG trước khi cho phép tầng công suất
   hoạt động. Mức an toàn phải do mạch ngoài giữ (pull-down/pull-up), vì lúc reset chân đang float.
2. **Fail-safe theo thời gian.** Không nhận lệnh mới trong N ms thì tự dừng. Driver có thể cung cấp
   cơ chế này (`*_set_watchdog_ms()`), nhưng ngưỡng N là **quyết định nghiệp vụ** — nhận qua config.
3. **Giới hạn ở driver.** Dải hợp lệ (duty tối đa, thời gian đóng tối đa của solenoid, tốc độ tối đa)
   kẹp ngay trong driver và trả `ESP_ERR_INVALID_ARG` khi vượt. Không tin tầng trên luôn đúng.
4. **Không có trạng thái "không rõ".** Sau lỗi, driver phải biết chắc đầu ra đang tắt, hoặc chuyển
   sang `ERROR` và báo lên. Không giả định lệnh cuối đã có hiệu lực.

## Relay và solenoid

- Bắt buộc có diode dập (flyback) song song cuộn dây — thiếu diode làm hỏng transistor và gây
  reset ngẫu nhiên. Nhắc user kiểm tra trước khi đổ lỗi cho firmware.
- Solenoid/van có **thời gian đóng tối đa liên tục** (nóng cuộn dây). Kẹp trong driver:
  quá thời gian thì tự ngắt, trả lỗi lên.
- Relay có thời gian chuyển mạch 5–15 ms và dội tiếp điểm: đừng đảo trạng thái liên tiếp
  nhanh hơn thế. Đặt thời gian tối thiểu giữa hai lần đổi trạng thái trong driver.
- Đóng/cắt tải cảm ứng gây nhiễu mạnh lên bus I2C/ADC gần đó. Nếu số đo nhảy đúng lúc relay
  đóng, đó là nhiễu phần cứng, không phải bug driver.

## Stepper

- Dùng RMT hoặc LEDC/MCPWM sinh xung STEP, **không** bit-bang bằng `vTaskDelay` — jitter tick
  làm mất bước và rung.
- Tăng/giảm tốc (ramp) là bắt buộc với tải quán tính; nhảy thẳng vào tốc độ cao sẽ trượt bước.
- Đếm bước để suy ra vị trí chỉ đúng khi không trượt bước. Cần vị trí tin cậy thì phải có
  công tắc hành trình (limit switch) hoặc encoder, và có quy trình về gốc (homing).
- Chân ENABLE của driver (A4988, DRV8825, TMC): giữ enable liên tục làm động cơ nóng khi đứng yên.
  Cho phép tầng trên tắt khi nghỉ.

## Motor DC / cầu H

- Dead-time bắt buộc khi tự lái cầu H — thiếu nó gây shoot-through, cháy MOSFET. Chi tiết
  trong `pwm-motor.md`.
- Không đảo chiều đột ngột khi đang quay: về 0, dừng một khoảng, rồi mới đổi chiều.
- Có đo dòng thì thêm phát hiện kẹt (stall): dòng cao kéo dài quá T ms → cắt và báo lỗi.

## Hợp đồng API gợi ý

```c
esp_err_t relay_create(const relay_config_t *cfg, relay_t **out); /* đặt mức an toàn */
esp_err_t relay_set(relay_t *r, bool on);        /* kẹp min_toggle_interval_ms */
esp_err_t relay_get(relay_t *r, bool *on);       /* trạng thái driver tin là đang có */
esp_err_t relay_feed_watchdog(relay_t *r);       /* không gọi trong N ms → tự tắt */
void      relay_emergency_stop(relay_t *r);      /* luôn thành công, gọi được mọi lúc */
```

`emergency_stop` phải là đường ngắn nhất tới trạng thái an toàn: không cấp phát, không chờ bus,
không thể trả lỗi.

## Test lần đầu

Nhắc user: tháo tải khỏi trục, cấp nguồn công suất qua nguồn có giới hạn dòng, và thử
`emergency_stop` cùng cơ chế fail-safe **trước** khi thử đường chạy bình thường.
