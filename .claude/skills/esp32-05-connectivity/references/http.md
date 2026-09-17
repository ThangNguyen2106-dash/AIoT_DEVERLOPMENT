# HTTP / HTTPS

## Client — khung đủ an toàn

```c
esp_http_client_config_t cfg = {
    .url = "https://api.example.com/v1/data",
    .crt_bundle_attach = esp_crt_bundle_attach,   /* xác thực server — KHÔNG bỏ qua */
    .timeout_ms = 10000,                          /* luôn đặt */
    .buffer_size = 2048,
    .buffer_size_tx = 1024,
    .disable_auto_redirect = false,
    .max_redirection_count = 3,
};
esp_http_client_handle_t cli = esp_http_client_init(&cfg);
if (!cli) return NET_FAIL_INIT;

esp_http_client_set_method(cli, HTTP_METHOD_POST);
esp_http_client_set_header(cli, "Content-Type", "application/json");
esp_http_client_set_post_field(cli, body, body_len);

net_fail_t cls = NET_OK;
esp_err_t err = esp_http_client_perform(cli);
if (err != ESP_OK) {
    cls = classify_transport(err);               /* timeout/DNS/TLS → LINK hoặc AUTH */
} else {
    int status = esp_http_client_get_status_code(cli);
    cls = classify_http_status(status);          /* 2xx OK, 401/403 AUTH, 4xx PROTO, 5xx SERVER */
}
esp_http_client_cleanup(cli);                    /* BẮT BUỘC, kể cả trên nhánh lỗi */
return cls;
```

Ba lỗi phải tránh, cả ba đều rất phổ biến:
1. **Không đặt `timeout_ms`** → task treo rất lâu, watchdog bắn.
2. **Chỉ kiểm tra `err == ESP_OK`** → HTTP 500 vẫn trả `ESP_OK`. Phải đọc status code.
3. **Không `cleanup()` ở nhánh lỗi** → rò socket và heap; chạy vài giờ là hết socket
   (`LWIP_MAX_SOCKETS`) rồi mọi kết nối fail với `ENOMEM`.

## Ánh xạ status → hành động

| Status | Lớp | Hành động |
|---|---|---|
| 200/201/204 | OK | xoá message khỏi buffer |
| 301/302/307/308 | — | để thư viện follow (đã bật), nhưng giới hạn số lần |
| 400/404/405/409/415/422 | PROTO | **bỏ message**, log payload rút gọn, tăng counter. Retry không bao giờ thành công. |
| 401/403 | AUTH | dừng, làm mới token nếu có luồng refresh; nếu không → FAULT, thử lại rất chậm |
| 408/425 | LINK | retry theo backoff |
| 429 | SERVER | đọc header `Retry-After`, chờ đúng số đó, không sớm hơn |
| 5xx | SERVER | backoff dài, **giữ nguyên buffer** |

`Retry-After` phải thực sự được đọc và tôn trọng; bỏ qua nó là cách bị cloud chặn.

## Response lớn và streaming

Không nạp cả response vào RAM. Với file/dữ liệu lớn, mở thủ công và đọc theo khối:

```c
esp_http_client_open(cli, 0);
int64_t len = esp_http_client_fetch_headers(cli);      /* -1 = chunked, không biết trước */
int status = esp_http_client_get_status_code(cli);
if (status / 100 != 2) { esp_http_client_close(cli); ... }

char buf[512];
int n;
while ((n = esp_http_client_read(cli, buf, sizeof buf)) > 0) {
    consume(buf, n);                                    /* ghi file / parse tăng dần */
}
if (n < 0) { /* LINK_FAIL giữa chừng — dữ liệu không toàn vẹn, đừng dùng một nửa */ }
esp_http_client_close(cli);
esp_http_client_cleanup(cli);
```

Ngắt giữa chừng là chuyện thường. Phải có cách biết dữ liệu đã đủ hay chưa (Content-Length,
checksum, hoặc parse tới khi hợp lệ); dùng một nửa response là bug im lặng.

## Tài nguyên

- Task gọi HTTPS cần stack ≥ 8 KB (TLS handshake tốn stack). Thiếu là stack overflow →
  `esp32-07-debugging`.
- Mỗi kết nối TLS tốn ~20–40 KB heap tuỳ `MBEDTLS_SSL_IN_CONTENT_LEN`. Không mở nhiều kết nối
  TLS đồng thời trên ESP32 nếu không đo heap trước.
- Nhiều request tới cùng host: đặt `.keep_alive_enable = true` và tái dùng handle — tránh bắt
  tay TLS lại mỗi lần (rất đáng giá với cellular).

## Xác thực

- Bearer token: `esp_http_client_set_header(cli, "Authorization", "Bearer ...")`. Token hết hạn
  → 401 → luồng refresh riêng, có trần số lần thử, không refresh vòng lặp.
- Basic auth chỉ chấp nhận được **trên HTTPS**.
- mTLS (client cert) → `tls.md`.
- Bí mật nằm ở đâu, bảo vệ thế nào → `esp32-10-security`. Không nhúng token vào source.

## HTTP server trên thiết bị

`esp_http_server` dùng cho trang cấu hình cục bộ, không phải để mở ra internet.

```c
httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
cfg.max_uri_handlers = 12;
cfg.recv_wait_timeout = 5;      /* giây — chặn client treo giữ socket */
cfg.send_wait_timeout = 5;
cfg.lru_purge_enable  = true;   /* đóng socket cũ khi hết chỗ, tránh cạn socket */
```

- Giới hạn kích thước body đọc vào; `httpd_req_recv()` theo vòng lặp có trần, đừng `malloc`
  theo `Content-Length` do client khai.
- Mọi handler phải validate đầu vào — đây là bề mặt tấn công mở cho bất kỳ ai trong cùng mạng.
- Handler chạy trong task của server: không blocking lâu, không chờ phần cứng chậm; đẩy việc
  sang task khác và trả 202 nếu cần.
- Bật tắt theo trạng thái: server cấu hình chỉ nên sống trong lúc provisioning, có timeout.
