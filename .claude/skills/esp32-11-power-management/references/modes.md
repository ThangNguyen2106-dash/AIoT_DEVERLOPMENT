# Các chế độ điện và cấu hình PM của IDF

Active → modem sleep → light sleep → deep sleep → hibernation. Mỗi bậc rẻ hơn về điện và
**đắt hơn về một thứ khác**. Chọn bậc theo yêu cầu ở bước 2 của quy trình, không chọn theo
"bậc nào tiết kiệm nhất".

## So sánh quyết định

| | Modem sleep | Auto light sleep | Light sleep thủ công | Deep sleep | Hibernation |
|---|---|---|---|---|---|
| CPU | chạy | dừng khi idle | dừng | tắt | tắt |
| RAM | giữ | giữ | giữ | **mất** (trừ RTC) | mất |
| Task/stack | giữ | giữ | giữ | mất — reboot | mất |
| Kết nối Wi-Fi/TCP | giữ | giữ | **mất** (trừ khi bật WIFI_PS và dùng auto) | mất | mất |
| Thời gian trở lại active | tức thì | tức thì | vài trăm µs | 200–500 ms (boot lại) | như deep sleep |
| Ngoại vi | chạy | phần lớn chạy | dừng | tắt | tắt |
| Giữ thời gian | có | có | có | RTC timer còn chạy | tuỳ cấu hình |
| Dùng khi | cần online liên tục | chu kỳ ngắn, cần phản hồi nhanh | chờ sự kiện vài giây–vài phút | chu kỳ dài (phút–giờ) | ngủ rất dài, không cần gì |

## Ngưỡng quyết định deep sleep vs light sleep

Deep sleep phải trả giá cố định mỗi lần thức: bootloader + init IDF + (nếu có mạng) kết nối lại
từ đầu. Với chu kỳ ngắn, **cái giá này lớn hơn phần tiết kiệm được**.

Quy tắc ước lượng:

```
E_deep(chu kỳ) = E_boot + E_việc + I_deep × T_ngủ
E_light(chu kỳ) =         E_việc + I_light × T_ngủ
→ deep sleep chỉ thắng khi:  T_ngủ > E_boot / (I_light − I_deep)
```

Với `E_boot ≈ 60 mA × 0.4 s = 24 mAs`, `I_light ≈ 0.8 mA`, `I_deep ≈ 0.02 mA`:
`T_ngủ > 24 / 0.78 ≈ 31 s`. Dưới khoảng đó, light sleep thường tốt hơn — và còn giữ được
kết nối, tức là **rẻ hơn mà vẫn ít đánh đổi hơn**.

Con số `E_boot` phải đo trên chính firmware của dự án, không dùng con số ví dụ ở đây: init
component nặng (PSRAM, filesystem, TLS context) có thể đẩy nó lên gấp nhiều lần.

## Modem sleep

Radio tắt giữa các beacon, CPU vẫn chạy, kết nối vẫn giữ. Bật bằng chính sách power save của
Wi-Fi, không phải API sleep riêng:

```c
esp_wifi_set_ps(WIFI_PS_MIN_MODEM);   /* thức theo DTIM của AP */
esp_wifi_set_ps(WIFI_PS_MAX_MODEM);   /* thức theo listen interval tự đặt — tiết kiệm hơn, trễ hơn */
esp_wifi_set_ps(WIFI_PS_NONE);        /* mặc định khi cần độ trễ thấp nhất */
```

Đánh đổi nằm ở downlink latency và tỉ lệ mất gói broadcast — chi tiết `wifi-power.md`.

## Automatic light sleep + DFS

IDF tự hạ tần số CPU và tự vào light sleep khi mọi task đều block:

```c
#include "esp_pm.h"
esp_pm_config_t pm = {
    .max_freq_mhz = 160,
    .min_freq_mhz = 40,        /* hoặc bằng tần số XTAL */
    .light_sleep_enable = true,
};
ESP_ERROR_CHECK(esp_pm_configure(&pm));
```
```
CONFIG_PM_ENABLE=y
CONFIG_FREERTOS_USE_TICKLESS_IDLE=y
CONFIG_PM_DFS_INIT_AUTO=y
```

Điều kiện để nó thật sự có tác dụng: **mọi task phải block thật**. Một task polling với
`vTaskDelay(1)` hoặc vòng lặp bận sẽ giữ hệ thống ở active và toàn bộ cấu hình trên thành vô
nghĩa. Kiểm tra bằng `vTaskGetRunTimeStats()` — IDLE phải chiếm phần lớn.

### Power lock — cấm ngủ khi đang làm việc dở

```c
esp_pm_lock_handle_t lock;
esp_pm_lock_create(ESP_PM_APB_FREQ_MAX, 0, "i2s", &lock);  /* giữ APB, cấm hạ tần số */
esp_pm_lock_acquire(lock);
/* ... thao tác nhạy timing ... */
esp_pm_lock_release(lock);
```

| Loại lock | Bảo vệ khỏi |
|---|---|
| `ESP_PM_CPU_FREQ_MAX` | hạ tần số CPU |
| `ESP_PM_APB_FREQ_MAX` | hạ tần số APB — ảnh hưởng timing ngoại vi |
| `ESP_PM_NO_LIGHT_SLEEP` | vào light sleep |

Driver IDF tự giữ lock khi cần, nhưng **code tự viết thì không**. Bit-bang, RMT, I2S, UART tốc độ
cao, đo xung — phải tự giữ lock, nếu không sẽ có lỗi timing chập chờn rất khó truy.

### Cạm bẫy đã biết của auto light sleep

| Vấn đề | Hệ quả |
|---|---|
| UART RX mất byte đầu | ký tự đầu tiên đánh thức chip nhưng bị mất; cần `uart_set_wakeup_threshold` và chấp nhận mất vài byte |
| Timing GPIO/bit-bang lệch | tần số APB đổi giữa chừng |
| Ngắt trễ hơn | thời gian đáp ứng tăng, ảnh hưởng đo xung |
| Đo thời gian bằng vòng lặp CPU sai | tần số thay đổi |

Tất cả những thứ này thuộc trục **Reliability** trong bảng đánh đổi — phải công bố, không được
bật `CONFIG_PM_ENABLE` rồi im lặng.

## Light sleep thủ công

```c
esp_sleep_enable_timer_wakeup(5ULL * 1000000ULL);
esp_sleep_enable_ext1_wakeup(BIT64(BUTTON_GPIO), ESP_EXT1_WAKEUP_ANY_HIGH);
esp_light_sleep_start();                      /* trả về sau khi thức */
esp_sleep_wakeup_cause_t why = esp_sleep_get_wakeup_cause();
```

Khác deep sleep ở chỗ **hàm trả về**, chương trình chạy tiếp — RAM, task, biến toàn cục còn
nguyên. Nhưng ngoại vi đã dừng: phải tự khôi phục những gì driver không tự làm (xem
`peripheral-power.md`).

Với Wi-Fi: kết nối có thể giữ qua light sleep nếu bật power save, nhưng socket TCP vẫn có thể
đứt nếu ngủ lâu hơn keepalive phía server. Đây là **mất kết nối im lặng** — thuộc trục
Reliability, phải xử lý ở `esp32-05-connectivity`.

## Deep sleep

```c
esp_sleep_enable_timer_wakeup(300ULL * 1000000ULL);
esp_deep_sleep_start();      /* KHÔNG BAO GIỜ trả về */
```

Thức dậy = chạy lại từ `app_main`. Mọi trạng thái trong RAM thường đã mất — xem
`deep-sleep-state.md`.

Trước khi gọi, module pm phải hoàn tất theo trật tự:

1. Mọi power lock đã nhả (không ai đang ghi flash, OTA, gửi dở).
2. Cơ cấu chấp hành ở trạng thái an toàn và được `gpio_hold_en` nếu cần giữ mức.
3. Session mạng đóng có trật tự (gửi trạng thái offline / để LWT làm việc).
4. Log đã flush.
5. Dữ liệu cần giữ đã vào RTC memory hoặc NVS.
6. Ngoại vi đã tắt nguồn, chân không dùng đã `rtc_gpio_isolate`.

Bỏ qua bước 1 hoặc 2 là hỏng thiết bị, không phải tiết kiệm điện.

## Hibernation

Tắt thêm cả RTC slow memory và phần lớn RTC peripheral, chỉ còn RTC timer. Cấu hình bằng
`esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF)` và tương tự cho các domain
khác.

Tiết kiệm thêm vài µA, đổi lại **mất `RTC_DATA_ATTR`** — nghĩa là mất luôn boot count, BSSID đã
ghim, buffer dữ liệu chưa gửi. Chỉ dùng khi đã xác nhận không cần giữ gì, và chấp nhận mỗi lần
thức là một lần khởi động lạnh hoàn toàn (kể cả quét Wi-Fi lại từ đầu — thường **tốn hơn** phần
đã tiết kiệm).

## Hạ tần số CPU

`min_freq_mhz`/`max_freq_mhz` trong `esp_pm_config_t`. Hạ tần số giảm dòng active nhưng **kéo dài
thời gian xử lý** — tổng năng lượng có khi không đổi hoặc tệ hơn (race to idle). Chỉ hạ khi đo
được lợi ích thật, và phải kiểm tra lại mọi driver nhạy timing.

## Ghi chú theo dòng chip

Dòng ngủ, danh sách domain tắt được, và khả năng của ULP/LP core khác nhau rõ rệt giữa
ESP32 / S2 / S3 / C3 / C6. **Tra datasheet của đúng target**, không suy từ ESP32 classic.
Với C6 và các chip có LP core, tồn tại lựa chọn "LP core xử lý, HP core ngủ" — thay đổi hoàn toàn
bài toán ngân sách; xem `wake-sources.md`.
