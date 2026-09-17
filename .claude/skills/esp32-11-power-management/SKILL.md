---
name: esp32-11-power-management
description: Tối ưu tiêu thụ điện cho ESP32 chạy pin — DFS, modem/light/deep sleep, hibernation, wake source (timer, EXT0/EXT1, touch, ULP, UART), RTC memory, cắt nguồn ngoại vi, duty cycle, hành vi điện của Wi-Fi/BLE, ngân sách pin, truy nguyên dòng rò. Mọi đề xuất phải công bố đánh đổi 4 trục Power/Latency/Reliability/Functionality. Dùng khi thiết bị hết pin nhanh hoặc thiết kế chu kỳ đo-gửi-ngủ.
---

# 11 — Power Management

## Điều kiện vào
Chưa rõ chip / framework / phiên bản IDF → chạy Context Gate ở `esp32-firmware` trước.
Cổng an toàn và ngân sách context của `esp32-firmware` áp dụng nguyên vẹn tại đây.

Chỉ dùng khi thiết bị chạy pin hoặc có ràng buộc công suất thật. Trên thiết bị cắm điện,
tối ưu điện là chi phí rủi ro không có lợi ích.

## Luật số 1: cấm đánh đổi âm thầm

Mọi kỹ thuật tiết kiệm điện đều lấy đi một thứ khác. Không bao giờ được áp dụng rồi im lặng.
Mỗi đề xuất phải kèm **bảng đánh đổi 4 trục**:

| Trục | Câu hỏi phải trả lời |
|---|---|
| **Power** | tiết kiệm bao nhiêu, ở dạng µA trung bình hay mAh/ngày — không nói "ít hơn nhiều" |
| **Latency** | thiết bị phản ứng chậm đi bao lâu: với sự kiện tại chỗ, và với lệnh từ server |
| **Reliability** | xác suất mất dữ liệu / mất kết nối / mất sự kiện tăng lên thế nào |
| **Functionality** | chức năng nào bị mất hoặc bị hạn chế (nhận lệnh, log, OTA, cảnh báo tức thời) |

Nếu một trục nào đó bị xấu đi mà người dùng chưa biết, thì đó là bug, không phải tối ưu.

**Đặc biệt cấm** những đánh đổi sau nếu người dùng không xác nhận rõ ràng:
- Tắt brownout detector hoặc watchdog để "đỡ reset". Đây là bỏ lưới an toàn, không phải tiết kiệm điện.
- Kéo dài DTIM/listen interval, keepalive MQTT hay chu kỳ kết nối làm thiết bị **không nhận
  được lệnh** trong hàng phút — trong khi người dùng vẫn tin là điều khiển được tức thời.
- Bỏ TLS, bỏ xác thực, bỏ đồng bộ thời gian để rút ngắn thời gian radio bật.
- Bỏ retry/buffer khi gửi thất bại để tiết kiệm thời gian phát sóng → mất dữ liệu im lặng.
- Giảm tần suất đo dưới mức mà yêu cầu nghiệp vụ hay quy định cho phép.
- Deep sleep ở thiết bị có cơ cấu chấp hành đang ở trạng thái nguy hiểm (van mở, bơm chạy).
- Hạ tần số CPU tới mức vỡ thời gian của driver (bit-bang, RMT, I2S, UART tốc độ cao).

## Luật số 2: bắt đầu bằng ngân sách, không bằng code

Chốt bốn con số trước khi chạm bất kỳ API sleep nào:

1. Dung lượng pin thực dùng được (mAh) — không phải con số in trên vỏ.
2. Tuổi thọ mong muốn (tháng/năm).
3. Chu kỳ hoạt động: đo bao lâu một lần, gửi bao lâu một lần.
4. Độ trễ phản ứng chấp nhận được với sự kiện và với lệnh từ xa.

→ Dòng trung bình mục tiêu = dung lượng / số giờ. Từ đó suy ngân sách mAs cho mỗi chu kỳ.
Không tối ưu mò. Cách lập ngân sách và mô hình năng lượng mỗi chu kỳ → `references/power-budget.md`.

## Luật số 3: đo trước khi sửa

Phần lớn "ESP32 ngủ mà vẫn ăn 12 mA" **không phải lỗi firmware**: LED nguồn, chip USB-UART,
LDO dòng nghỉ cao, cảm biến không được cắt nguồn, chân float. Sửa code trong trường hợp đó là
tốn công vô ích. Danh sách truy nguyên theo thứ tự → `references/peripheral-power.md`.

Đo bằng power profiler (PPK2, Otii) hoặc shunt + oscilloscope. Đồng hồ vạn năng không bắt được
đỉnh dòng ngắn và cho kết quả sai lệch lớn — không kết luận dựa trên số của nó.

## Bậc thang chế độ

| Chế độ | Dòng tham khảo | Giữ được gì | Đánh đổi chính |
|---|---|---|---|
| Active + Wi-Fi TX | 100–500 mA (đỉnh cao hơn) | tất cả | — |
| Modem sleep | 20–40 mA | CPU, RAM, kết nối Wi-Fi còn (theo DTIM) | latency downlink tăng theo DTIM |
| Auto light sleep (DFS + PM) | 1–5 mA | RAM, task, kết nối | timing ngoại vi lệch; UART RX có thể mất byte |
| Light sleep thủ công | ~0.8 mA | RAM và trạng thái task | ngoại vi dừng; phải chủ động khôi phục |
| Deep sleep | 5–20 µA | chỉ RTC memory | **reboot từ đầu**, mất mọi kết nối |
| Hibernation | ~2–5 µA | gần như không gì | như trên, thêm mất RTC memory tuỳ cấu hình |

Số liệu thay đổi theo dòng chip, module và điều kiện — đưa ra làm tham khảo, **luôn yêu cầu đo
thực tế**, không hứa con số cụ thể với người dùng. Chi tiết từng chế độ → `references/modes.md`.

## Quy trình 8 bước

| # | Bước | Đầu ra |
|---|---|---|
| 1 | **Ngân sách** | mAh, tuổi thọ, dòng trung bình mục tiêu, ngân sách mAs mỗi chu kỳ |
| 2 | **Yêu cầu không được phá** | độ trễ tối đa với sự kiện/lệnh, dữ liệu không được mất, chức năng luôn phải sống |
| 3 | **Đo hiện trạng** | biểu đồ dòng theo thời gian của một chu kỳ đầy đủ, chia theo pha |
| 4 | **Chọn chế độ** | active/light/deep cho từng pha, kèm lý do dựa trên bước 2 |
| 5 | **Wake source** | nguồn đánh thức + chân RTC hợp lệ + xử lý wake cause |
| 6 | **Giữ trạng thái** | cái gì vào `RTC_DATA_ATTR`, cái gì xuống NVS, cái gì chấp nhận mất |
| 7 | **Cắt nguồn ngoại vi** | cảm biến/module nào tắt được, cắt bằng gì, thời gian khởi động lại |
| 8 | **Công bố đánh đổi** | bảng 4 trục cho từng thay đổi + số liệu đo lại sau khi áp dụng |

Bước 8 là bắt buộc, không phải tuỳ chọn.

## Hợp đồng của module quản lý điện

```c
/* pm.h — một nơi duy nhất quyết định khi nào được ngủ */
typedef enum { PM_ACTIVE, PM_IDLE, PM_LIGHT_SLEEP, PM_DEEP_SLEEP } pm_mode_t;

/* Khoá chống ngủ: thành phần nào đang làm việc dở thì giữ khoá. */
esp_err_t pm_lock_acquire(const char *owner);   /* OTA, đang gửi, đang ghi flash, actuator bật */
esp_err_t pm_lock_release(const char *owner);
uint32_t  pm_lock_count(void);

esp_err_t pm_request_sleep(pm_mode_t mode, uint64_t us);  /* từ chối nếu còn khoá */
esp_sleep_wakeup_cause_t pm_last_wake_cause(void);
esp_err_t pm_get_stats(pm_stats_t *out);        /* số chu kỳ, thời gian active, mAs ước tính */
```

Quy ước không được phá:
- Không có hàm nào trong ứng dụng được gọi thẳng `esp_deep_sleep_start()`. Chỉ module pm gọi,
  và chỉ khi mọi khoá đã nhả. Ngủ giữa lúc đang ghi flash hoặc đang OTA là hỏng thiết bị.
- Cơ cấu chấp hành đang ở trạng thái không an toàn thì **giữ khoá**, kể cả khi hết pin gần kề.
- Trước khi deep sleep: đóng session mạng có trật tự (LWT/offline message), flush log, đưa
  ngoại vi về trạng thái đã biết.
- Mọi lần thức phải đọc và xử lý `wakeup_cause`, kể cả trường hợp không mong đợi
  (`ESP_SLEEP_WAKEUP_UNDEFINED` = reset thật, không phải thức dậy).
- Có "chế độ bảo dưỡng": một đường để thiết bị **không ngủ** trong X phút sau khi bật hoặc sau
  khi nhấn nút, đủ cho OTA và gỡ lỗi. Thiếu nó thì thiết bị ngủ sâu ngoài hiện trường là không
  còn cập nhật được nữa.

## Định tuyến reference

| Cần làm | Reference |
|---|---|
| Lập ngân sách, mô hình năng lượng chu kỳ, chọn pin, đo đạc | `references/power-budget.md` |
| Chi tiết từng chế độ, `esp_pm_config`, DFS, power lock của IDF | `references/modes.md` |
| Timer/EXT0/EXT1/GPIO/touch/ULP/LP core/UART wake, chân RTC theo chip | `references/wake-sources.md` |
| RTC memory, NVS wear, giữ thời gian, khôi phục trạng thái sau reboot | `references/deep-sleep-state.md` |
| Cắt nguồn cảm biến, MOSFET, pull-up, `rtc_gpio_isolate`, GPIO hold, duty cycle cảm biến | `references/peripheral-power.md` |
| Wi-Fi PS/DTIM/listen interval, rút ngắn thời gian kết nối, MQTT/BLE và điện | `references/wifi-power.md` |
| Bảng đánh đổi sẵn cho từng kỹ thuật, câu hỏi phải hỏi người dùng | `references/tradeoffs.md` |
| Rà soát trước khi coi là xong | `checklists/power-review.md` |

## Mẫu báo cáo

```
## Ngân sách
Pin: <2000 mAh Li-ion, hiệu dụng ~1700 mAh>   Tuổi thọ mục tiêu: <12 tháng>
→ Dòng trung bình cho phép: <~194 µA>
Chu kỳ: <đo 5 phút/lần, gửi 30 phút/lần>   Độ trễ chấp nhận được: <sự kiện 1 s, lệnh 30 phút>

## Hiện trạng đo được
| Pha | Dòng | Thời gian | mAs/chu kỳ |
|---|---|---|---|
| boot + init | 60 mA | 0.4 s | 24 |
| đo cảm biến | 25 mA | 1.2 s | 30 |
| Wi-Fi connect + gửi | 120 mA | 4.5 s | 540 |
| deep sleep | 18 µA | 294 s | 5.3 |
Tổng: <...> mAs/chu kỳ → dòng trung bình <...> µA → tuổi thọ ước tính <...>

## Thay đổi đề xuất và ĐÁNH ĐỔI
| Thay đổi | Power | Latency | Reliability | Functionality |
|---|---|---|---|---|
| Ghim BSSID+kênh vào RTC mem | −380 mAs/chu kỳ | tốt hơn 2.5 s | giảm nhẹ: AP đổi kênh thì lần đầu fail, cần fallback quét | không đổi |
| Gửi gộp 6 mẫu/30 phút | −450 mAs/chu kỳ | dữ liệu lên cloud trễ tối đa 30 phút | buffer RTC mất nếu mất nguồn | cảnh báo tức thời phải có đường riêng |
| DTIM 10 thay vì 3 | −0.6 mA khi online | lệnh từ server trễ tới ~1 s | AP có thể bỏ broadcast | không đổi |

## Rủi ro và điều KHÔNG làm
<liệt kê những tối ưu đã cân nhắc nhưng từ chối, kèm lý do>

## Cách kiểm chứng
<đo lại bằng gì, ở pha nào, số liệu kỳ vọng>
```

## Không thuộc scope

- Chọn chip, tính dòng đỉnh cho nguồn, chọn LDO/DC-DC, thiết kế mạch cắt nguồn → `esp32-02-hardware-analysis`
- Cấu hình Wi-Fi/MQTT, reconnect, buffering offline → `esp32-05-connectivity`
- Lịch đo và logic nghiệp vụ của chu kỳ, fail-safe cơ cấu chấp hành → `esp32-06-application-development`
- Thiết bị reset ngẫu nhiên, brownout đã xảy ra → `esp32-07-debugging`
- Giảm RAM, tăng tốc xử lý, thu nhỏ binary → `esp32-09-performance-optimization`
- OTA khi thiết bị ngủ nhiều, cửa sổ cập nhật → `esp32-12-release`

## Đầu ra
Thiết kế chu kỳ năng lượng có số: bảng ngân sách, chế độ cho từng pha, wake source, chiến lược
giữ trạng thái, cách cắt nguồn ngoại vi — kèm **bảng đánh đổi 4 trục** và cách đo kiểm chứng.
