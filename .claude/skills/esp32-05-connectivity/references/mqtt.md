# MQTT

## Cấu hình client

```c
esp_mqtt_client_config_t cfg = {
    .broker.address.uri = "mqtts://broker.example.com:8883",
    .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
    .credentials.client_id = device_id,          /* PHẢI duy nhất từng thiết bị */
    .credentials.username = user,
    .credentials.authentication.password = pass, /* hoặc client cert cho mTLS */
    .session.keepalive = 30,
    .session.disable_clean_session = true,       /* giữ subscription qua lần rớt */
    .session.last_will = {
        .topic = "dev/xxx/status", .msg = "offline", .msg_len = 7, .qos = 1, .retain = 1,
    },
    .network.timeout_ms = 10000,
    .network.reconnect_timeout_ms = 5000,        /* xem mục "tự reconnect" bên dưới */
    .buffer.size = 2048,
    .task.stack_size = 6144,                     /* 8192 nếu có TLS */
};
esp_mqtt_client_handle_t c = esp_mqtt_client_init(&cfg);
if (!c) return NET_FAIL_INIT;
```

## Client id — nguồn lỗi kinh điển

Hai thiết bị trùng `client_id` sẽ đá nhau khỏi broker liên tục; triệu chứng là "kết nối được
rồi rớt ngay" lặp vô hạn, và nó trông y hệt lỗi mạng. Lấy id từ MAC hoặc serial trong NVS,
không bao giờ hard-code một chuỗi dùng chung cho cả lô hàng.

## Đọc CONNACK — đừng gộp mọi lỗi kết nối

`MQTT_EVENT_ERROR` mang `error_handle`, phải phân loại:

```c
case MQTT_EVENT_ERROR: {
    esp_mqtt_error_codes_t *e = evt->error_handle;
    if (e->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
        /* lỗi socket/TLS: e->esp_transport_sock_errno, e->esp_tls_last_esp_err */
        net_post_err(NET_EVT_DOWN, classify_socket(e->esp_transport_sock_errno), ...);
    } else if (e->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
        switch (e->connect_return_code) {
        case MQTT_CONNECTION_REFUSE_PROTOCOL:     /* 0x01 */
        case MQTT_CONNECTION_REFUSE_ID_REJECTED:  /* 0x02 */
            net_post_err(NET_EVT_DOWN, NET_FAIL_PROTO, e->connect_return_code); break;
        case MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE: /* 0x03 */
            net_post_err(NET_EVT_DOWN, NET_FAIL_SERVER, ...); break;
        case MQTT_CONNECTION_REFUSE_BAD_USERNAME: /* 0x04 */
        case MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED: /* 0x05 */
            net_post_err(NET_EVT_DOWN, NET_FAIL_AUTH, ...); break;
        }
    }
    break;
}
```

CONNACK 0x04/0x05 mà vẫn để client tự reconnect mỗi 5 giây là cách nhanh nhất để bị cloud chặn
thiết bị. Gặp AUTH: `esp_mqtt_client_stop()`, chuyển state, hẹn thử lại sau ≥ 5 phút.

## Tự reconnect của thư viện: dùng hay tự làm

`esp-mqtt` có sẵn reconnect theo `reconnect_timeout_ms` (cố định, không backoff, không phân
loại lỗi). Hai lựa chọn, chọn rõ ràng chứ đừng để nửa vời:

1. **Dùng sẵn** cho dự án đơn giản: chấp nhận khoảng cách cố định, nhưng vẫn phải bắt
   `MQTT_EVENT_ERROR` để `stop()` khi gặp AUTH/PROTO.
2. **Tự quản** cho production: `cfg.network.disable_auto_reconnect = true`, rồi tự gọi
   `esp_mqtt_client_reconnect()` theo backoff của mình (`connection-contract.md` §4). Cách này
   cho phép áp đúng chính sách theo 5 lớp lỗi và đồng bộ với state machine link.

Không được vừa bật auto-reconnect vừa tự gọi reconnect — hai bộ đếm đá nhau, tần suất thật
không đoán được.

## QoS, LWT, session

- Luôn đặt **Last Will and Testament** để hệ thống phía sau biết thiết bị mất kết nối; kèm
  publish `"online"` retained khi vào READY.
- QoS 0 cho telemetry tần suất cao; QoS 1 cho lệnh và sự kiện quan trọng. QoS 2 hiếm khi đáng
  chi phí trên nhúng.
- QoS 1 là *at least once*: bên nhận **sẽ** thấy bản trùng. Mỗi message có id tăng dần để khử trùng.
- `clean_session = false` giữ subscription và message QoS1 phía broker khi thiết bị offline —
  hữu ích cho lệnh điều khiển, nhưng broker phải chịu được lượng hàng đợi đó, và thiết bị sẽ
  nhận một loạt message cũ ngay khi vào lại. Xử lý được cơn lũ đó (hoặc bỏ message quá hạn theo
  timestamp) là phần thiết kế, không phải tuỳ chọn.
- Lệnh điều khiển retained là bẫy: thiết bị reboot sẽ nhận lại lệnh cũ từ tuần trước và chạy nó.
  Lệnh phải có timestamp/hạn hiệu lực và thiết bị phải từ chối lệnh quá hạn.

## Publish khi mất kết nối

`esp_mqtt_client_publish()` lúc chưa CONNECTED trả −1 (hoặc vào outbox nếu bật). Không coi đó
là thành công.

- Tầng ứng dụng **không** gọi publish trực tiếp: đẩy vào queue có giới hạn (`connection-contract.md` §6).
- Task mạng xả queue khi `READY`, có tiết chế (ví dụ ≤ 10 msg/s), và chỉ xoá khỏi queue khi:
  QoS 0 → sau khi `publish()` trả id ≥ 0; QoS 1 → sau khi nhận `MQTT_EVENT_PUBLISHED` đúng id.
  Xoá sớm là mất dữ liệu ngay tại thời điểm mạng chập chờn — đúng lúc dữ liệu cần nhất.
- Outbox nội bộ của esp-mqtt (`CONFIG_MQTT_OUTBOX_EXPIRED_TIMEOUT_MS`) chỉ là lưới đỡ; nó nằm
  trong RAM và mất khi reboot. Đừng coi nó là buffer offline.

## Nhận dữ liệu

- Callback MQTT chạy trong task của client: **không** làm việc nặng hay blocking ở đó. Copy
  payload, đẩy queue, thoát. Con trỏ `evt->data` không còn hợp lệ sau khi callback trả về.
- Message lớn đến theo nhiều phần: xử lý `MQTT_EVENT_DATA` với `current_data_offset` và
  `total_data_len`; đừng giả định một event là một message trọn vẹn. Nếu `total_data_len` lớn
  hơn buffer đã dự trù → bỏ có kiểm soát và đếm, không tràn bộ nhớ.
- Payload từ broker là **đầu vào không tin cậy**: kiểm tra độ dài, kiểm tra JSON parse được,
  kiểm tra dải giá trị trước khi chạm cơ cấu chấp hành (→ `esp32-10-security`).

## Chẩn đoán nhanh

| Triệu chứng | Nguyên nhân hay gặp |
|---|---|
| Connect rồi rớt ngay, lặp vô hạn | trùng `client_id` |
| Connect fail ngay từ đầu, luôn luôn | sai user/pass (AUTH) hoặc sai cổng TLS/không TLS |
| Chạy tốt rồi rớt đúng chu kỳ ~keepalive | task mạng bị chặn, không gửi được PINGREQ |
| Publish trả −1 dù đang online | payload lớn hơn `buffer.size`, hoặc outbox đầy |
| Crash trong callback | dùng `evt->data` sau khi callback trả về, hoặc stack task MQTT quá nhỏ với TLS |
