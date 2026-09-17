# CPU, độ trễ, jitter, IRAM và cache

## Phân biệt ba vấn đề khác nhau

| Vấn đề | Nghĩa là | Đo bằng |
|---|---|---|
| **Throughput CPU** | không đủ chu kỳ để làm hết việc | `vTaskGetRunTimeStats` |
| **Độ trễ** (latency) | từ sự kiện tới lúc phản ứng quá lâu | GPIO toggle + scope |
| **Jitter** | thời gian phản ứng **không ổn định** | scope, xem phân bố chứ không xem trung bình |

Ba vấn đề này có ba cách sửa khác nhau. "Chậm" chung chung thì chưa khoanh được nút thắt.
Hệ thống thời gian thực hỏng vì **trường hợp xấu nhất**, không vì trung bình — luôn lấy max.

## Tìm task ăn CPU

```
CONFIG_FREERTOS_USE_TRACE_FACILITY=y
CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y
```
```c
char buf[1024];
vTaskGetRunTimeStats(buf);       /* % CPU theo task */
ESP_LOGI(TAG, "\n%s", buf);
```

Đọc kết quả:
- `IDLE` gần 0% trên một core → core đó quá tải, watchdog sắp bắt.
- Một task chiếm phần lớn → tối ưu đúng task đó, đừng tối ưu chỗ khác.
- Không task nào nổi bật nhưng vẫn trễ → vấn đề là **độ trễ/jitter**, không phải throughput.

## ISR: ngắn, và chỉ ngắn

- ISR chỉ: đọc thanh ghi / đọc dữ liệu, đẩy vào queue hoặc gửi task notification, thoát.
  Mọi xử lý nặng làm ở task (quy tắc đầy đủ: `esp32-03/references/coding-rules.md`).
- ISR dài làm trễ **mọi** ISR khác và có thể kích hoạt Interrupt WDT.
- Ngắt mức cao (high-level interrupt) chỉ dùng khi thật sự cần µs — viết khó, dễ sai, hiếm khi đáng.
- Độ trễ ISR thực tế chỉ đo được bằng **GPIO toggle + scope**, không đo được bằng log.

## IRAM và cache — nguồn jitter lớn nhất bị bỏ qua

Code chạy từ flash đi qua cache. **Cache miss = dừng vài µs.** Tệ hơn: khi có thao tác ghi
flash (NVS, OTA) hoặc trên một số đường code SPI flash, **cache bị tắt tạm thời** — mọi code
nằm ở flash đều không chạy được trong khoảng đó.

Hệ quả thực tế:
- Ghi NVS trong lúc đang phát xung thời gian thực → xung bị méo. Đây là bug hay gặp và rất
  khó tìm nếu không biết cơ chế.
- ISR đăng ký với cờ `ESP_INTR_FLAG_IRAM` **bắt buộc** `IRAM_ATTR` cho hàm ISR **và mọi hàm nó gọi**,
  và không được chạm dữ liệu nằm ở flash (chuỗi hằng!).

Quy tắc dùng `IRAM_ATTR`:

| Đặt IRAM_ATTR | Không đặt |
|---|---|
| ISR phải chạy kể cả khi cache tắt | hàm chỉ "muốn nhanh hơn" |
| Hàm được ISR đó gọi | hàm gọi một lần lúc init |
| Vòng lặp cực nóng đã **đo** là bị cache miss | đoán là nóng |

IRAM là tài nguyên rất hạn chế — gắn bừa sẽ gây lỗi link (`binary-size.md`).
Mỗi lần gắn phải có số chứng minh.

## Tần số CPU

`CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ` (80 / 160 / 240 tuỳ chip).

- Tăng lên 240 MHz là cách rẻ nhất để có thêm CPU — nhưng **tăng tiêu thụ điện đáng kể**.
  Thiết bị chạy pin → phải qua `esp32-11-power-management`, không tự quyết ở đây.
- **Hạ tần số làm vỡ timing của driver** bit-bang, RMT, I2S, UART tốc độ cao. Nếu bật DFS
  (`esp_pm_config`), mọi driver nhạy timing phải giữ power lock.

## Jitter: nguyên nhân theo thứ tự hay gặp

1. Log trong đường nóng (UART blocking).
2. Ghi flash/NVS làm tắt cache.
3. Task priority ngang nhau tranh CPU, hoặc priority inversion (thiết kế → `esp32-03`;
   đã xảy ra → `esp32-07-debugging/references/rtos-faults.md`).
4. ISR khác chạy dài.
5. Dùng `vTaskDelay` để định nhịp — độ phân giải bằng tick (thường 10 ms).
   Cần nhịp chính xác: `esp_timer` (µs) hoặc ngoại vi phần cứng (LEDC/RMT/MCPWM), không phải delay.

## Việc chọn nhịp

| Cần | Dùng | Không dùng |
|---|---|---|
| Nhịp ms, không khắt khe | `vTaskDelayUntil` | `vTaskDelay` (trôi dần) |
| Nhịp µs, chính xác | `esp_timer` (callback chạy ở ngữ cảnh riêng) | `esp_rom_delay_us` (busy-wait) |
| Xung đều tuyệt đối | LEDC / RMT / MCPWM — phần cứng tự phát | bất kỳ vòng lặp phần mềm nào |

Tăng priority để "kịp deadline" là biện pháp bị cấm ở SKILL.md — nó thường chỉ đẩy vấn đề
sang task khác. Nếu thật sự cần đổi priority, đó là quyết định kiến trúc → `esp32-03`.
