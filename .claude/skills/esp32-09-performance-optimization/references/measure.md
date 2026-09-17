# Đo: chỉ số nào, công cụ nào, đo thế nào cho đúng

## Đo sai còn tệ hơn không đo

| Lỗi đo | Hậu quả |
|---|---|
| Đo lúc idle, Wi-Fi chưa kết nối | thiếu 40-50 KB heap mà TLS/Wi-Fi sẽ lấy → kết luận "còn dư" là sai |
| Đo `esp_get_free_heap_size()` một lần | bỏ sót đáy; phải dùng `esp_get_minimum_free_heap_size()` |
| Đo stack ngay sau khi tạo task | chưa chạy nhánh sâu nhất → high-water mark vô nghĩa |
| Đo thời gian bằng `ESP_LOGI` | bản thân log qua UART tốn hàng ms, làm lệch cái đang đo |
| Đo với mức log VERBOSE rồi ship bằng INFO | hai firmware khác nhau, số không so sánh được |

Nguyên tắc: **đo dưới tải thực, ở trạng thái xấu nhất, trên firmware giống bản sẽ ship.**

## Bộ chỉ số tối thiểu

```c
ESP_LOGI(TAG, "heap free=%u min=%u largest=%u",
         (unsigned) esp_get_free_heap_size(),
         (unsigned) esp_get_minimum_free_heap_size(),
         (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
ESP_LOGI(TAG, "%s stack high-water=%u B",
         pcTaskGetName(NULL),
         (unsigned) uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t));
```

`uxTaskGetStackHighWaterMark` trả về **số word**, không phải byte — nhân `sizeof(StackType_t)`
(4 trên ESP32). Quên nhân là nguồn của rất nhiều kết luận sai.

Đặt một "health log" định kỳ (60 s một lần, mức INFO) in heap min + high-water mark của các
task chính. Nó vừa là dữ liệu tối ưu, vừa là dữ liệu chẩn đoán từ xa cho `esp32-12-release`.

## Công cụ theo mục đích

| Cần biết | Công cụ | Bật bằng |
|---|---|---|
| Heap còn bao nhiêu, ai cấp phát | `heap_caps_print_heap_info()`, heap trace | `CONFIG_HEAP_TRACING_STANDALONE=y` |
| Stack mỗi task dùng bao nhiêu | `uxTaskGetStackHighWaterMark` | mặc định có |
| Task nào ăn CPU | `vTaskGetRunTimeStats()` | `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y` |
| Danh sách task, trạng thái, prio | `vTaskList()` | `CONFIG_FREERTOS_USE_TRACE_FACILITY=y` |
| Binary chiếm chỗ ở đâu | `idf.py size`, `size-components`, `size-files` | mặc định có |
| Thời gian một đoạn code | `esp_timer_get_time()` (µs, `int64_t`) | mặc định có |
| Jitter / độ trễ thật | GPIO toggle + oscilloscope / logic analyzer | phần cứng |
| Dòng thời gian đa task | SystemView, ESP-IDF app trace | `CONFIG_APPTRACE_*` |

Các cờ `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS` và heap tracing **có chi phí runtime**.
Bật để đo, tắt trước khi ship, và nêu rõ số đo lấy ở cấu hình nào.

## Đo thời gian đúng cách

```c
int64_t t0 = esp_timer_get_time();
do_work();
int64_t dt = esp_timer_get_time() - t0;      /* µs */
```

- Đo **nhiều lần và lấy max**, không lấy trung bình: deadline bị phá bởi trường hợp xấu nhất.
- Đo trong điều kiện có cả Wi-Fi, có cả ngắt — đo lúc hệ thống rỗi cho số đẹp vô nghĩa.
- Cần chính xác hơn `esp_timer` hoặc cần thấy jitter thật: **toggle GPIO và soi bằng scope**.
  Đây là cách duy nhất thấy được độ trễ ISR và jitter thực sự.

## Trước khi kết luận "hết RAM"

Phân biệt ba tình huống khác nhau, ba cách sửa khác nhau:

| Quan sát | Nghĩa là | Đi tiếp |
|---|---|---|
| `min free` giảm đều theo thời gian | **rò bộ nhớ** — đây là bug | `esp32-07-debugging` |
| `min free` ổn định nhưng thấp | dùng nhiều thật | `memory.md` |
| `free` nhiều nhưng `largest block` nhỏ | **phân mảnh** | `memory.md` |

Đừng tối ưu tình huống 1 — nó là lỗi, không phải giới hạn tài nguyên.
