# PWM và điều khiển động cơ

## LEDC — LED, servo, PWM đơn giản

```c
ledc_timer_config_t t = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .timer_num = LEDC_TIMER_0,
    .duty_resolution = LEDC_TIMER_13_BIT,
    .freq_hz = 5000,
    .clk_cfg = LEDC_AUTO_CLK,
};
ESP_ERROR_CHECK(ledc_timer_config(&t));
```

Ràng buộc: `freq_hz * 2^duty_resolution` không được vượt clock nguồn. Tần số càng cao thì
độ phân giải càng phải giảm. `ledc_timer_config` trả lỗi nếu tổ hợp không hợp lệ — đừng bỏ qua.

Servo RC: 50 Hz, xung 1.0–2.0 ms. Với 13-bit ở 50 Hz thì duty tương ứng khoảng 410–820.

## MCPWM — cầu H, BLDC, cần dead-time

Bắt buộc cấu hình **dead-time** khi lái cầu H. Thiếu dead-time thì hai nhánh dẫn cùng lúc
(shoot-through) và làm cháy MOSFET. Đây là lỗi phá hỏng phần cứng, không chỉ là bug phần mềm.

## An toàn khi điều khiển cơ cấu chấp hành

- Khởi tạo chân ở trạng thái DỪNG trước khi bật driver công suất.
- Fail-safe bằng phần mềm: nếu không nhận lệnh mới trong N ms thì tự động dừng. Không để
  motor chạy tiếp khi mất liên lạc với bộ điều khiển.
- Ramp tốc độ, không nhảy 0 đến 100% duty tức thì với tải quán tính lớn hoặc nguồn yếu
  (gây sụt áp và brownout reset).
- Nhắc người dùng: lần test đầu nên tháo tải khỏi trục motor.
- Có encoder hoặc cảm biến dòng thì thêm phát hiện kẹt (stall) và ngắt, tránh cháy motor.
