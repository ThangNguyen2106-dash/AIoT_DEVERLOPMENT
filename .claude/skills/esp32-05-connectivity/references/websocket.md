# WebSocket client

Dùng khi cần kênh hai chiều, độ trễ thấp, server chủ động đẩy dữ liệu. Nếu chỉ đẩy telemetry
lên thì MQTT thường hợp hơn (có QoS, LWT, retained sẵn).

Component: `esp_websocket_client` (managed component `espressif/esp_websocket_client`).

```c
esp_websocket_client_config_t cfg = {
    .uri = "wss://api.example.com/ws",
    .crt_bundle_attach = esp_crt_bundle_attach,
    .network_timeout_ms = 10000,
    .reconnect_timeout_ms = 5000,
    .disable_auto_reconnect = true,     /* tự quản backoff — xem connection-contract.md §4 */
    .ping_interval_sec = 20,
    .pingpong_timeout_sec = 10,         /* không có pong trong 10 s → coi như chết */
    .buffer_size = 2048,
    .task_stack = 6144,
    .headers = "Authorization: Bearer xxx\r\n",
};
esp_websocket_client_handle_t ws = esp_websocket_client_init(&cfg);
if (!ws) return NET_FAIL_INIT;
```

## Handshake — chỗ phân biệt PROTO với LINK

WebSocket bắt đầu bằng một HTTP request và **phải** nhận `101 Switching Protocols`.

| Kết quả handshake | Lớp | Nghĩa |
|---|---|---|
| 101 | OK | |
| 401/403 | AUTH | token sai/hết hạn — không retry nhanh |
| 400/404 | PROTO | sai đường dẫn, sai subprotocol, server không có endpoint đó |
| 200 (không nâng cấp) | PROTO | endpoint không phải WebSocket, hoặc proxy chặn nâng cấp |
| 5xx | SERVER | backoff dài |
| timeout / reset trước khi có phản hồi | LINK | mạng |

Đọc status từ `WEBSOCKET_EVENT_ERROR` (`esp_ws_handshake_status_code` trong data khi có) và
phân loại; đừng gộp tất cả thành "connect fail".

## Chết câm — vấn đề đặc trưng của WebSocket

TCP có thể còn "mở" trong khi phía kia đã biến mất (NAT hết hạn, AP rớt, server bị kill). Ứng
dụng sẽ ngồi chờ dữ liệu mãi mãi mà không có lỗi nào. Ping/pong là cách phát hiện duy nhất:

- Bật `ping_interval_sec` và `pingpong_timeout_sec`. Chu kỳ ping phải **ngắn hơn** thời gian
  NAT timeout của mạng (thường 60–300 s; cellular có khi 30 s).
- Ngoài ping của giao thức, giữ thêm mốc "lần cuối nhận được byte bất kỳ". Quá ngưỡng thì đóng
  chủ động và reconnect, kể cả khi thư viện chưa báo lỗi.
- Server cũng có thể ping mình; thư viện tự trả pong. Đừng tự xử lý opcode ping trong ứng dụng.

## Gửi và nhận

```c
int sent = esp_websocket_client_send_text(ws, json, len, pdMS_TO_TICKS(5000));
if (sent < 0) { /* LINK_FAIL — giữ message trong buffer, KHÔNG coi là đã gửi */ }
```

- Luôn có timeout khi gửi; `portMAX_DELAY` sẽ khoá task khi mạng nghẽn.
- Sự kiện `WEBSOCKET_EVENT_DATA`: message có thể đến theo nhiều fragment — dùng
  `payload_offset`, `payload_len`, `data_len` để ghép; kiểm tra `op_code`
  (0x1 text, 0x2 binary, 0x8 close, 0x9 ping, 0xA pong). Fragment quá lớn so với buffer dự trù
  → bỏ có kiểm soát và đếm.
- Nhận `op_code 0x8` (close) kèm close code: 1000 là đóng bình thường (server chủ động, không
  phải lỗi), 1008/1011 là từ chối/lỗi server. Log close code — nó nói đúng nguyên nhân.
- Callback chạy trong task của client: copy dữ liệu, đẩy queue, thoát.

## Reconnect

Tự quản (`disable_auto_reconnect = true`) rồi áp backoff theo lớp lỗi. Sau khi nối lại:
- Phải **đăng ký lại** mọi thứ phía server yêu cầu (subscribe kênh, gửi lại auth frame,
  khôi phục vị trí stream). WebSocket không có khái niệm session bền như MQTT
  `clean_session=false` — server không nhớ giúp.
- Nếu giao thức ứng dụng có số thứ tự message, gửi lại từ mốc cuối đã được server xác nhận,
  không phải từ mốc cuối đã gửi.
