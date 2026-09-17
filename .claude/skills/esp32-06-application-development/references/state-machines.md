# State machine nghiệp vụ

## Vì sao phải là state machine

Logic điều khiển viết bằng cờ rải rác (`bool is_running, has_error, is_waiting;`) sinh ra
tổ hợp trạng thái không ai liệt kê hết — và chính những tổ hợp không ai nghĩ tới là chỗ
thiết bị làm điều nguy hiểm. State machine buộc phải nói rõ: **có đúng những state nào**,
và **từ mỗi state đi được đâu**.

Dùng khi: hành vi phụ thuộc lịch sử, có timeout, có retry, có chuỗi bước phải đúng thứ tự.
Không dùng khi: hàm chỉ ánh xạ input sang output (khi đó viết hàm thuần là đủ).

## Khuôn test được trên host

Chìa khoá: hàm chuyển trạng thái **không làm gì cả** — nó chỉ trả về trạng thái mới và
**danh sách hành động** cho tầng gọi thực hiện.

```c
/* pump_ctrl.h — không include driver, không include FreeRTOS */
typedef enum { PUMP_IDLE, PUMP_PRIMING, PUMP_RUNNING, PUMP_COOLDOWN, PUMP_FAULT } pump_state_t;

typedef enum { EV_TICK, EV_LEVEL_LOW, EV_LEVEL_OK, EV_SENSOR_FAIL, EV_CMD_STOP } pump_event_t;

typedef struct {
    bool     set_motor;      /* hành động, không phải lời gọi driver */
    bool     motor_on;
    bool     raise_alarm;
    uint32_t alarm_code;
} pump_action_t;

typedef struct {
    pump_state_t state;
    uint32_t     state_entered_ms;
    uint8_t      fail_count;
} pump_ctx_t;

/* Hàm THUẦN: cùng (ctx, event, now, cfg) luôn cho cùng kết quả. */
pump_state_t pump_step(pump_ctx_t *ctx, pump_event_t ev, uint32_t now_ms,
                       const pump_cfg_t *cfg, pump_action_t *out);
```

Tầng gọi (chạm I/O, không chứa quyết định):

```c
pump_action_t act = {0};
pump_step(&ctx, ev, now_ms, &cfg, &act);
if (act.set_motor)  motor_set(motor_dev, act.motor_on);   /* driver */
if (act.raise_alarm) alarm_report(act.alarm_code);        /* mạng */
```

Host test khi đó là C thuần, chạy trên máy tính, không cần chip:

```c
/* "bơm phải tự dừng sau max_run_ms kể cả khi mức nước chưa đạt" */
ctx = (pump_ctx_t){ .state = PUMP_RUNNING, .state_entered_ms = 0 };
pump_step(&ctx, EV_TICK, cfg.max_run_ms + 1, &cfg, &act);
assert(ctx.state == PUMP_COOLDOWN);
assert(act.set_motor && act.motor_on == false);
```

## Quy tắc

- **Thời gian là tham số, không phải lời gọi.** Truyền `now_ms` vào; không gọi
  `xTaskGetTickCount()` bên trong — nếu gọi thì không test được timeout mà không chờ thật.
- **Cấu hình là tham số**, không phải `#define`. Cho phép test nhiều cấu hình.
- **Mọi state phải có đường ra.** State không có đường ra là thiết bị treo im lặng.
- **Mọi state có timeout** (trừ state nghỉ có chủ đích). Đây là cách bắt được tình huống
  "phần cứng không phản hồi" mà không cần biết nó hỏng thế nào.
- **`FAULT` phải có đường phục hồi** — hoặc tự thoát sau N phút, hoặc thoát bằng can thiệp.
  `FAULT` là ngõ cụt thì mỗi lỗi nhỏ thành một chuyến đi tới hiện trường.
- **State vào được trạng thái nguy hiểm thì hành động an toàn phải nằm ở lối VÀO**, không phải
  lối ra: nếu thiết bị reset giữa chừng, chỉ lối vào mới chắc chắn chạy.

## Bảng chuyển trạng thái — viết trước khi viết code

| State | Sự kiện | State mới | Hành động | Timeout |
|---|---|---|---|---|
| IDLE | LEVEL_LOW | PRIMING | motor on | 10 s → FAULT |
| PRIMING | LEVEL_OK | RUNNING | — | max_run_ms → COOLDOWN |
| PRIMING | SENSOR_FAIL | FAULT | motor off, alarm | — |
| RUNNING | LEVEL_OK | COOLDOWN | motor off | 60 s → IDLE |
| RUNNING | TICK ≥ max_run | COOLDOWN | motor off, alarm | 60 s → IDLE |
| FAULT | TICK ≥ 5 phút | IDLE | — | — |

Ô trống trong bảng = một sự kiện đến lúc thiết bị chưa biết phải làm gì.
Quy tắc mặc định: sự kiện không khớp hàng nào thì **bỏ qua và đếm**, không bao giờ crash,
và **không bao giờ im lặng đổi state**.

## Nhiều state machine

Một thiết bị thường có vài state machine song song: kết nối mạng
(`net_state_t` ở `esp32-05-connectivity`), điều khiển thiết bị, chế độ nguồn
(`esp32-11-power-management`). Giữ chúng **tách rời**, giao tiếp qua sự kiện.

Gộp thành một state machine "tổng" là cách nhanh nhất để có 40 state không ai hiểu.
