# Wake source — đánh thức từ light/deep sleep

Lỗi phổ biến nhất trong toàn bộ chủ đề power: **chọn chân không phải RTC GPIO làm wake source**.
Biểu hiện là thiết bị ngủ và không bao giờ dậy — ngoài hiện trường thì trông y hệt pin hỏng.

## Bảng chọn

| Nguồn | API | Chân | Deep | Light | Ghi chú |
|---|---|---|---|---|---|
| Timer | `esp_sleep_enable_timer_wakeup(us)` | — | có | có | luôn dùng được, chính xác theo RTC clock |
| Một chân, mức xác định | `esp_sleep_enable_ext0_wakeup(gpio, level)` | RTC GPIO | có | có | giữ RTC peripheral bật → tốn thêm dòng |
| Nhiều chân | `esp_sleep_enable_ext1_wakeup(mask, mode)` | RTC GPIO | có | có | rẻ hơn ext0; `ANY_HIGH` / `ALL_LOW` tuỳ chip |
| GPIO (C3/C6/S2/S3) | `esp_deep_sleep_enable_gpio_wakeup()` | RTC/LP GPIO | có | có | thay cho ext0/ext1 trên các chip mới |
| GPIO trong light sleep | `gpio_wakeup_enable()` + `esp_sleep_enable_gpio_wakeup()` | GPIO thường được | không | có | light sleep linh hoạt hơn nhiều |
| Touch | `esp_sleep_enable_touchpad_wakeup()` | chân touch | có | có | không có trên mọi dòng chip |
| ULP / LP core | `esp_sleep_enable_ulp_wakeup()` | — | có | có | xem mục ULP bên dưới |
| UART | `uart_set_wakeup_threshold()` + `esp_sleep_enable_uart_wakeup()` | UART0/1 | không | có | **mất vài byte đầu** |
| Wi-Fi / BT | `esp_sleep_enable_wifi_wakeup()` | — | không | có | dùng với auto light sleep để giữ kết nối |

**Luôn tra datasheet của đúng target** để biết chân nào là RTC GPIO. Danh sách khác nhau giữa
ESP32 / S2 / S3 / C3 / C6, và module không đưa hết chân ra header — đối chiếu
`esp32-02-hardware-analysis/references/pin-constraints.md`.

## Mẫu chuẩn: nhiều nguồn đánh thức + xử lý lý do

```c
#define WAKE_PINS  (BIT64(GPIO_NUM_25) | BIT64(GPIO_NUM_26))

static void configure_wakeup(void)
{
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(300ULL * 1000000ULL));
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup(WAKE_PINS, ESP_EXT1_WAKEUP_ANY_HIGH));
}

static void handle_wakeup(void)
{
    switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_TIMER:
        do_scheduled_measurement();
        break;
    case ESP_SLEEP_WAKEUP_EXT1: {
        uint64_t mask = esp_sleep_get_ext1_wakeup_status();   /* chân nào đã kích */
        handle_event(mask);
        break;
    }
    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
        /* KHÔNG phải thức dậy: đây là power-on, reset, hoặc panic.
           Phải khởi tạo lại toàn bộ trạng thái, không tin RTC memory. */
        cold_start();
        break;
    }
}
```

`ESP_SLEEP_WAKEUP_UNDEFINED` bị bỏ qua là bug hay gặp: sau một lần brownout hoặc panic, thiết bị
chạy tiếp như thể vừa thức dậy bình thường, dùng dữ liệu RTC memory đã rác. Xem
`deep-sleep-state.md` phần kiểm tra tính hợp lệ.

## Chân nút nhấn làm wake source

| Vấn đề | Xử lý |
|---|---|
| Nảy phím (bounce) | debounce phần cứng (RC), hoặc chấp nhận thức nhiều lần và lọc bằng phần mềm |
| Chân float khi ngủ | bắt buộc có pull xác định; pull nội phải bật qua RTC domain (`rtc_gpio_pullup_en`) |
| Người giữ nút lâu | thiết bị thức → ngủ → thức lại liên tục; cần chờ nhả nút trước khi ngủ lại |
| Pull nội không sống qua deep sleep | dùng pull **ngoài**, hoặc `esp_sleep_pd_config` giữ RTC peripheral (tốn dòng) |

Giữ nút bị kẹt cơ khí ở mức kích hoạt là kịch bản làm cạn pin nhanh nhất trong thực tế —
phải có giới hạn số lần thức liên tiếp trong khoảng thời gian ngắn, rồi bỏ qua nguồn đó
một lúc (và **báo lên server**, không im lặng).

## ext0 vs ext1

| | ext0 | ext1 |
|---|---|---|
| Số chân | 1 | nhiều (bitmask) |
| Dòng tiêu thụ khi ngủ | cao hơn — cần giữ RTC peripheral | thấp hơn |
| Giữ được pull nội khi ngủ | có (vì RTC peripheral bật) | không, trừ khi bật domain |
| Điều kiện | mức 0 hoặc 1 | ANY_HIGH / ALL_LOW (tuỳ chip) |

Mặc định nên dùng **ext1** vì rẻ hơn về điện; chỉ dùng ext0 khi cần đúng một chân với pull nội
được giữ và đã tính phần dòng tăng thêm vào ngân sách.

## Đánh thức bằng UART

Chỉ hoạt động ở light sleep. Chip thức khi thấy đủ số cạnh trên RX:

```c
uart_set_wakeup_threshold(UART_NUM_0, 3);
esp_sleep_enable_uart_wakeup(UART_NUM_0);
```

**Các byte đầu tiên bị mất** — không tránh được, vì chip chưa chạy khi chúng đến. Giao thức phải
chịu được: dùng ký tự đánh thức bỏ đi, hoặc preamble, hoặc để phía kia gửi hai lần. Nếu giao thức
không đổi được (Modbus của thiết bị có sẵn), thì UART wake **không dùng được** — nói rõ điều này
thay vì để mất khung dữ liệu rải rác.

## ULP và LP core — xử lý khi CPU chính ngủ

Cho phép đo/lọc ở mức µA mà không đánh thức CPU chính. Đây là kỹ thuật tiết kiệm mạnh nhất cho
thiết bị phải theo dõi liên tục.

| Chip | Coprocessor | Lập trình bằng |
|---|---|---|
| ESP32 | ULP FSM | assembly ULP |
| S2 / S3 | ULP FSM + ULP RISC-V | C (RISC-V) hoặc assembly |
| C6 | LP core (RISC-V) | C, có driver LP I2C/UART riêng |

Mẫu dùng điển hình: ULP đọc ADC mỗi giây, so ngưỡng, chỉ đánh thức CPU chính khi vượt ngưỡng.
CPU chính ngủ 99.9% thời gian.

Đánh đổi phải công bố:
- **Functionality**: ULP rất hạn chế — bộ nhớ nhỏ, không có float, không có thư viện, không mạng.
- **Reliability**: code ULP khó test và khó gỡ lỗi; bug trong ULP biểu hiện là "thiết bị không
  bao giờ báo cáo", rất khó truy ngoài hiện trường.
- **Chi phí phát triển** cao hơn hẳn — chỉ đáng khi ngân sách bắt buộc.

Giao tiếp ULP ↔ CPU chính qua RTC slow memory; phải có cơ chế kiểm tra tính hợp lệ như mọi dữ
liệu qua ranh giới (xem `deep-sleep-state.md`).

## Kiểm tra bắt buộc trước khi chốt wake source

- [ ] Mọi chân dùng làm wake là RTC/LP GPIO của **đúng target** (tra datasheet, không suy đoán).
- [ ] Có **ít nhất một** nguồn đánh thức không phụ thuộc thế giới bên ngoài — thường là timer.
      Chỉ dựa vào GPIO/ULP mà logic đó hỏng thì thiết bị ngủ vĩnh viễn.
- [ ] `wakeup_cause` được xử lý đầy đủ, kể cả `UNDEFINED`.
- [ ] Có chống thức liên tục (nút kẹt, cảm biến rung liên tục) và có báo cáo tình trạng đó.
- [ ] Có đường vào "chế độ bảo dưỡng" không ngủ, để còn OTA và gỡ lỗi được ngoài hiện trường.
- [ ] Mức pull của chân wake được xác định bằng phần cứng, hoặc bằng domain RTC đã tính vào ngân sách.
