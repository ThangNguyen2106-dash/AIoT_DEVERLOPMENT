# Stack overflow, heap corruption, memory leak, phân mảnh

Phục vụ lớp lỗi: tràn stack, hỏng heap, rò bộ nhớ, phân mảnh.
Bốn thứ này triệu chứng giống nhau (crash ngẫu nhiên, ở chỗ khác nhau mỗi lần) nhưng cách xác
minh khác hẳn — **phân biệt trước, sửa sau**.

## Phân biệt nhanh

| Quan sát | Nghi | Xác minh bằng |
|---|---|---|
| Crash ngay sau khi vào một hàm cụ thể, task luôn cố định | tràn stack | `uxTaskGetStackHighWaterMark` |
| `Stack canary watchpoint triggered (<task>)` | tràn stack | đã xác nhận, tìm khung stack lớn |
| `CORRUPT HEAP`, crash trong `malloc`/`free` | hỏng heap | heap poisoning |
| Chạy tốt N giờ rồi crash, free heap giảm đều | rò bộ nhớ | đồ thị free heap theo thời gian |
| Free heap còn nhiều nhưng `malloc` khối lớn trả NULL | phân mảnh | `largest_free_block` |
| Crash ở vị trí khác nhau mỗi lần, backtrace vô nghĩa | hỏng bộ nhớ (stack hoặc heap) | cả hai hướng trên |

## Thước đo nền — in định kỳ cho mọi firmware sẽ chạy dài

```c
ESP_LOGI(TAG, "heap free=%u min=%u largest=%u",
         (unsigned)esp_get_free_heap_size(),
         (unsigned)esp_get_minimum_free_heap_size(),
         (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
ESP_LOGI(TAG, "stack hwm=%u", (unsigned)uxTaskGetStackHighWaterMark(NULL));
```

Không có chuỗi số này theo thời gian thì **không kết luận được** rò hay phân mảnh — hãy yêu cầu
người dùng bật rồi chạy lại, đừng đoán.

---

## 1. Stack overflow

### Triệu chứng
- `***ERROR*** A stack overflow in task <name> has been detected.`
- `Stack canary watchpoint triggered (<task>)` / `Unhandled debug exception`.
- Hoặc **không có thông báo nào** nếu `CONFIG_FREERTOS_CHECK_STACKOVERFLOW=none` — khi đó chỉ
  thấy crash lung tung, backtrace rác.

### Bật phát hiện trước khi điều tra
```
CONFIG_FREERTOS_CHECK_STACKOVERFLOW_CANARY=y      # rẻ, phát hiện sau khi đã tràn
CONFIG_FREERTOS_CHECK_STACKOVERFLOW_PTRVAL=y      # hoặc watchpoint: bắt đúng lúc tràn
CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK=y         # chính xác nhất, dừng ngay tại lệnh ghi tràn
```

### Xác minh
```c
UBaseType_t hwm = uxTaskGetStackHighWaterMark(handle);  /* đơn vị: word trên IDF, không phải byte */
vTaskList(buf);   /* cột Stack = high-water mark còn lại của từng task */
```
High-water mark là **phần còn thừa ít nhất từ trước tới giờ**. Dưới ~10% stack cấp phát là vùng
nguy hiểm; dưới vài chục byte là sắp tràn.

Lưu ý: high-water mark chỉ phản ánh **những nhánh code đã chạy**. Nhánh hiếm (xử lý lỗi, bản tin
lớn, TLS handshake) chưa chạy thì con số đẹp vẫn không chứng minh được an toàn.

### Nguyên nhân thường gặp
| Nguyên nhân | Dấu hiệu |
|---|---|
| Mảng cục bộ lớn (`char buf[4096]`) | tràn chỉ khi vào đúng nhánh đó |
| `printf`/`snprintf`/log format phức tạp | ngốn vài trăm byte ~ vài KB |
| JSON/XML parser đệ quy | tràn theo độ sâu dữ liệu đầu vào |
| TLS/mbedTLS handshake | cần stack lớn (thường ≥ 8KB cho task làm TLS) |
| Đệ quy không giới hạn | tràn tất định, nhanh |
| Callback chạy trong task hệ thống (Wi-Fi/event/timer) | tràn **stack của task đó**, không phải task mình |

Callback của `esp_event`, `esp_timer`, Wi-Fi driver chạy trên stack task hệ thống — làm việc nặng
trong callback là nguyên nhân tràn hay bị bỏ sót. Đúng cách: callback chỉ đẩy việc vào queue.

### Minimal Fix
- Chuyển buffer lớn từ stack sang heap, hoặc `static` nếu chỉ một task dùng và không tái nhập.
- Bỏ đệ quy hoặc chặn độ sâu.
- Đẩy việc nặng ra khỏi callback hệ thống.
- **Tăng stack chỉ hợp lệ khi** đã tính ra nhu cầu thật (khung lớn nhất + dự phòng) và con số cũ
  vốn không đủ ngay từ đầu — kèm số liệu, không phải "cho chắc".

---

## 2. Heap corruption

### Triệu chứng
- `CORRUPT HEAP: Bad head at 0x... Expected 0xabba1234 got 0x...`
- `heap_caps_free ... was not allocated`, `assert failed: multi_heap_free`
- Crash bên trong `malloc`/`free`/`realloc` — **thủ phạm không ở đó**, nó chỉ là nơi phát hiện.

### Xác minh — heap poisoning
```
CONFIG_HEAP_POISONING_COMPREHENSIVE=y      # ghi mẫu canary quanh mỗi block, kiểm tra khi free
CONFIG_HEAP_TRACING_STANDALONE=y           # nếu cần truy ai cấp phát
```
Rồi chèn kiểm tra tại các mốc nghi ngờ để **thu hẹp vùng gây hỏng**:
```c
heap_caps_check_integrity_all(true);   /* true = in lỗi ra log */
```
Đặt lời gọi này trước/sau từng giai đoạn, nhị phân dần cho tới khi khoanh được đoạn code làm hỏng.

Poisoning làm chậm đáng kể — **tắt trước khi ship**.

### Nguyên nhân thường gặp
| Nguyên nhân | Mô tả |
|---|---|
| Ghi tràn buffer heap | `memcpy` quá kích thước, `strcpy` không kiểm tra, off-by-one |
| Double free | giải phóng hai lần, hoặc hai chủ sở hữu cùng free |
| Use-after-free | dùng lại con trỏ sau `free`, callback giữ con trỏ đã huỷ |
| Free con trỏ không do malloc | free biến stack/static, hoặc free con trỏ đã bị cộng offset |
| Trộn API cấp phát | `heap_caps_malloc` với `free` sai vùng, `new`/`free` lẫn lộn |
| Ghi từ ISR vào buffer đang bị giải phóng | thiếu đồng bộ giữa ISR và task |

### Minimal Fix
Xác định **một chủ sở hữu duy nhất** cho mỗi vùng nhớ và một điểm giải phóng duy nhất.
Gán `NULL` ngay sau `free` để double free chuyển thành lỗi im lặng-an toàn (không thay cho việc
sửa quyền sở hữu). Thiết kế lại quyền sở hữu: `esp32-03-firmware-architecture`.

---

## 3. Memory leak

### Triệu chứng
Chạy tốt rồi crash sau vài giờ/ngày; `esp_get_free_heap_size()` giảm đơn điệu;
`esp_get_minimum_free_heap_size()` tụt dần qua mỗi chu kỳ.

### Xác minh
Đo free heap **tại cùng một điểm trong chu kỳ** (ví dụ đầu mỗi vòng gửi dữ liệu), không đo ngẫu
nhiên — heap dao động trong chu kỳ là bình thường, cái đáng lo là **đáy tụt dần**.

```c
#include "esp_heap_trace.h"
static heap_trace_record_t s_rec[100];
heap_trace_init_standalone(s_rec, 100);
heap_trace_start(HEAP_TRACE_LEAKS);
/* ... chạy một chu kỳ nghi ngờ ... */
heap_trace_stop();
heap_trace_dump();      /* in các block cấp phát mà chưa free, kèm callstack */
```
Cần `CONFIG_HEAP_TRACING_STANDALONE=y` và `CONFIG_HEAP_TRACING_STACK_DEPTH` ≥ 4 để có callstack.

### Nguyên nhân thường gặp
- Đường lỗi không giải phóng: `return` sớm sau `malloc` mà quên `free`.
- Tạo task/timer/queue/event handler lặp lại mà không xoá (mỗi lần reconnect tạo mới một client).
- Client MQTT/HTTP/TLS khởi tạo lại trong vòng reconnect mà không `destroy`.
- `strdup`/`cJSON_Parse`/`asprintf` không có `free` tương ứng.
- Buffer nhận dữ liệu cấp theo bản tin, chỉ free ở nhánh thành công.

Rò trong vòng reconnect mạng là trường hợp phổ biến nhất trên ESP32 — kiểm tra
`esp32-05-connectivity` phần vòng đời client.

### Minimal Fix
Đóng đúng tài nguyên ở **mọi đường thoát** của hàm, kể cả đường lỗi. Với vòng reconnect: tạo
client một lần, tái sử dụng — không huỷ/tạo mỗi lần mất kết nối nếu API cho phép reconnect.

---

## 4. Phân mảnh heap

### Triệu chứng
`esp_get_free_heap_size()` vẫn lớn nhưng cấp phát khối lớn thất bại; TLS handshake hoặc buffer ảnh
không cấp được sau một thời gian chạy, dù ban đầu chạy tốt.

### Xác minh
```c
size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
```
`free` lớn nhưng `largest` nhỏ và **nhỏ dần theo thời gian** = phân mảnh, không phải rò.

### Minimal Fix
- Cấp phát các buffer dài hạn **một lần lúc init**, giữ luôn, không malloc/free lặp lại.
- Dùng buffer tĩnh hoặc pool cho đối tượng có kích thước cố định, tần suất cao.
- Tách vùng: buffer lớn vào PSRAM (`MALLOC_CAP_SPIRAM`) nếu có, để heap nội không bị băm nhỏ.
- Tránh `realloc` tăng dần từng chút.

Tối ưu bố trí bộ nhớ ở mức thiết kế: `esp32-09-performance-optimization`.

---

## Khi nghi hỏng bộ nhớ nhưng chưa biết stack hay heap

Thứ tự rẻ → đắt:
1. Bật `CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK` + `CONFIG_HEAP_POISONING_COMPREHENSIVE`,
   chạy lại — hai cái này thường tự chỉ đích danh.
2. In `vTaskList()` + heap stats định kỳ, chạy tới lúc crash, so hai đường cong.
3. Tắt dần từng task/tính năng cho tới khi lỗi biến mất → thu hẹp vùng nghi.
4. Còn bí: core dump (`coredump-debug`) để xem stack của **mọi** task tại thời điểm crash.
