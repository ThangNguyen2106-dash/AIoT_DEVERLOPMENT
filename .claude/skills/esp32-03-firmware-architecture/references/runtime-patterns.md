# Runtime: task, giao tiếp, event, state machine

## Task architecture

Một task cho **một luồng hoạt động độc lập**, tức là có nhịp riêng hoặc block trên
nguồn sự kiện riêng. Không tạo task cho "một chức năng".

Dấu hiệu cần task mới:
- Block trên nguồn khác (queue khác, socket, timer khác nhịp).
- Yêu cầu thời gian thực khác hẳn (đọc ADC 1 kHz vs gửi MQTT mỗi phút).
- Có thể chặn lâu và không được làm kẹt phần còn lại.

Dấu hiệu **không** cần: hai task luôn chạy nối tiếp nhau → gộp lại.
Task chỉ để "cho gọn code" → đó là hàm, không phải task.

### Priority và core

| Mức | Dùng cho |
|---|---|
| 18–24 | reserved cho stack Wi-Fi/BLE của IDF — không chen vào |
| 10–15 | task thời gian thực ngắn, phản hồi ISR |
| 5–9 | task nghiệp vụ thông thường |
| 1–4 | nền: log, housekeeping, gửi số liệu |

- Priority cao = deadline chặt, **không phải** = quan trọng.
- Task priority cao phải block sớm, không busy-loop, không giữ khoá lâu.
- Core affinity: chỉ ghim khi có lý do (core 0 bận radio → ghim việc timing-critical
  sang core 1). Không rõ lý do thì `tskNO_AFFINITY`.
- Ngân sách stack ban đầu: 2048 task đơn giản, 3072 có log/format, 4096+ nếu chạm
  TLS/JSON/file. Đây là phỏng đoán để chạy được; đo thật bằng
  `uxTaskGetStackHighWaterMark` → `esp32-09-performance-optimization`.

## Giao tiếp giữa task

| Cơ chế | Dùng khi | Không dùng khi |
|---|---|---|
| Task notification | 1 producer → 1 consumer, tín hiệu hoặc 1 giá trị 32-bit | nhiều consumer, cần hàng đợi dữ liệu |
| Queue | truyền dữ liệu, cần đệm, tách nhịp hai bên | dữ liệu lớn (truyền con trỏ + quy ước sở hữu) |
| Event group | chờ tổ hợp điều kiện (đã có IP VÀ đã có giờ) | truyền dữ liệu |
| Mutex | bảo vệ tài nguyên dùng chung (bus I2C, cấu hình) | đồng bộ trình tự — dùng queue |
| Semaphore đếm | N tài nguyên giống nhau, hoặc ISR → task đếm sự kiện | thay cho mutex |
| `esp_event` | phát tán 1 sự kiện tới nhiều bên quan tâm | đường dữ liệu tần suất cao |

Quy tắc:
- **Truyền qua queue theo giá trị** nếu dữ liệu nhỏ. Truyền con trỏ thì phải ghi rõ
  bên nào giải phóng — trong hợp đồng interface.
- Không chia sẻ biến giữa task mà không có khoá; `volatile` không phải cơ chế đồng bộ.
- Mỗi tài nguyên dùng chung có **một chủ sở hữu**. Ưu tiên: một task sở hữu bus và
  nhận yêu cầu qua queue > nhiều task tranh nhau mutex.
- Thứ tự khoá cố định toàn dự án nếu có hơn một mutex. Tốt nhất: đừng giữ hai mutex
  cùng lúc.
- Hành vi khi queue đầy phải quyết định trước: chặn, bỏ mẫu cũ, hay bỏ mẫu mới.
  Không để mặc định "block vô hạn" ở đường dữ liệu.

## ISR

Xem `coding-rules.md`. Tóm tắt kiến trúc: ISR **không chứa logic**. Nó đọc phần cứng,
đẩy vào queue hoặc gửi notification, xong. Mọi quyết định nằm ở task.

## Event system

Chỉ thêm khi **một sự kiện có nhiều bên quan tâm**, hoặc **bên phát không nên biết bên nhận**.
Hai task với một queue trực tiếp thì không cần event bus.

Nếu cần:
- Dùng `esp_event` của IDF, đừng tự viết.
- Handler chạy trên task của event loop → phải ngắn, không block. Việc nặng đẩy sang
  task của mình.
- Định nghĩa event base cho từng module; payload là struct POD, copy theo giá trị.
- Không dùng event để gọi một hàm cụ thể ở một nơi cụ thể — đó là gọi hàm, hãy gọi hàm.
- Nguy cơ chính: mất tính deterministic. Nếu thứ tự handler quan trọng thì thiết kế
  đang sai — event phải độc lập thứ tự.

## State machine

Cần khi hành vi phụ thuộc lịch sử, hoặc có timeout/retry/reconnect. Đây là chỗ `if`
lồng nhau thối rữa nhanh nhất.

Mức simple: `if/else`. Mức medium trở lên:

```c
typedef enum { ST_IDLE, ST_MEASURING, ST_SENDING, ST_FAULT } state_t;
typedef enum { EV_TICK, EV_DATA_READY, EV_TX_DONE, EV_ERROR } event_t;

/* hàm thuần: không I/O, không log, test được trên host */
state_t sm_next(state_t s, event_t e, action_t *out_action);
```

Quy tắc:
- Một task sở hữu một state machine. State không được sửa từ task khác.
- Hàm chuyển trạng thái là **hàm thuần**: `(state, event) → state + action`.
  I/O nằm ngoài, do caller thực thi. Đây là thứ làm nó test được.
- Mọi state phải có đường ra, kể cả state lỗi. Không có ngõ cụt.
- Mỗi state chờ đợi phải có timeout. Không timeout = treo im lặng ngoài thực địa.
- Log mọi transition ở mức DEBUG; transition vào FAULT log mức WARN/ERROR.
- Vẽ bảng chuyển trạng thái đầy đủ trước khi code; ô trống là ô chưa nghĩ tới.

State machine nghiệp vụ cụ thể → `esp32-06-application-development`.
Reconnect/backoff mạng → `esp32-05-connectivity`.
