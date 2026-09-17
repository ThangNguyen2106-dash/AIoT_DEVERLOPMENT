# TLS, chứng chỉ và thời gian

## Luật cứng

**Không bao giờ tắt xác thực chứng chỉ để "cho chạy được".** Không `skip_cert_common_name_check`,
không `use_secure_element = false` kiểu bỏ verify, không nhét CA rỗng. Nếu người dùng đang gỡ
lỗi TLS, nhiệm vụ là tìm nguyên nhân thật (CA thiếu, giờ sai, SNI, cert hết hạn) chứ không phải
đề xuất bỏ verify — kể cả "chỉ để test", vì cấu hình đó luôn theo lên sản phẩm.

## Ba nguyên nhân của 95% lỗi handshake

### 1. Giờ hệ thống sai

ESP32 khởi động ở 1970. Verify cert sẽ fail với `X509_CERT_VERIFY_FAILED (-0x2700)` và thông
báo không nhắc gì tới thời gian. **Đồng bộ SNTP trước khi bắt tay TLS lần đầu.**

```c
esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
cfg.start = true;
cfg.sync_cb = time_synced_cb;
esp_netif_sntp_init(&cfg);

if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) != ESP_OK) {
    /* LINK_FAIL: chưa có giờ → CHƯA được vào READY nếu transport cần TLS */
}
```

Ràng buộc thiết kế kéo theo:
- Điều kiện vào `READY` của link có TLS phải gồm "giờ đã đồng bộ ít nhất một lần".
- Lưu timestamp cuối vào RTC memory/NVS để sau reboot có mốc thô, thay vì quay lại 1970.
- Thiết bị dùng deep sleep: RTC trôi; đồng bộ lại định kỳ (→ `esp32-11-power-management`).
- Mạng bị chặn UDP 123 là chuyện có thật trong nhà máy — dự trù lấy giờ từ HTTP `Date` header
  hoặc từ payload của server như phương án hai.

### 2. Thiếu CA đúng

Hai cách, chọn theo nhu cầu:

```c
/* A. CA bundle của ESP-IDF: tin cậy các root CA phổ biến. Tốn ~64 KB flash. */
.crt_bundle_attach = esp_crt_bundle_attach,

/* B. Ghim đúng CA của mình (private CA hoặc pinning): nhỏ hơn, chặt hơn. */
.cert_pem = (const char *)server_root_ca_pem_start,
```

- Ghim CA gốc riêng là lựa chọn tốt cho hệ thống tự vận hành broker: nhẹ, và không tin cả thế giới.
- Ghim **leaf certificate** thì tuyệt đối tránh trừ khi có quy trình cập nhật chắc chắn — cert
  leaf đổi mỗi 90 ngày (Let's Encrypt) là thiết bị chết hàng loạt.
- CA gốc cũng **sẽ hết hạn trong vòng đời sản phẩm**. Phải có đường cập nhật CA qua OTA ngay từ
  đầu — thiết kế lúc này tốn vài giờ, thiếu nó thì phải thu hồi thiết bị.

### 3. SNI và tên miền

Nhiều host dùng chung IP. Không gửi SNI đúng → server trả cert sai → verify fail. Các client
của ESP-IDF tự lấy SNI từ hostname trong URL; nhưng nếu kết nối bằng **IP thuần** thì không có
SNI và verify theo tên sẽ fail. Hệ quả: dùng hostname, đừng dùng IP, với mọi endpoint TLS.

## mTLS (client certificate)

```c
.client_cert_pem = (const char *)client_crt_start,
.client_key_pem  = (const char *)client_key_start,
```

- Khoá riêng của thiết bị là bí mật quan trọng nhất trên board. Nơi lưu và cách bảo vệ (NVS mã
  hoá, eFuse, secure element ATECC608) → `esp32-10-security`.
- Server từ chối client cert biểu hiện là `SSL_FATAL_ALERT_MESSAGE` (alert 42/48) giữa
  handshake → phân loại **AUTH_FAIL**, không retry nhanh.
- Cert thiết bị có hạn: cần luồng gia hạn trước khi hết hạn, và cảnh báo lên cloud khi sắp hết.

## Tài nguyên

| Mục | Con số thực tế |
|---|---|
| Heap mỗi kết nối TLS | ~20–45 KB, chủ yếu là buffer in/out |
| Stack task chạy handshake | ≥ 8 KB |
| Thời gian handshake | 1–3 s trên Wi-Fi, 3–10 s trên cellular |
| Dữ liệu handshake | ~5–8 KB (quan trọng với SIM tính theo MB) |

Giảm khi thiếu RAM: `CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN` / `OUT_CONTENT_LEN` từ 16384 xuống
4096–8192 — **chỉ khi** server chấp nhận record nhỏ (hầu hết có, qua max_fragment_length hoặc
vì record thực tế nhỏ). Đặt quá nhỏ mà server gửi record lớn sẽ fail ở giữa phiên, rất khó truy.

Bật session resumption (tickets) để lần nối lại rẻ hơn nhiều — đặc biệt đáng giá với thiết bị
reconnect thường xuyên và thiết bị cellular.

## Phân loại lỗi TLS

| Lỗi | Lớp | Nguyên nhân thật |
|---|---|---|
| `-0x2700` `X509_CERT_VERIFY_FAILED` | AUTH | thiếu CA, **giờ sai**, cert hết hạn, sai tên miền |
| alert 42 `bad_certificate` / 48 `unknown_ca` | AUTH | server từ chối client cert (mTLS) |
| `-0x7280` `SSL_CONN_EOF` giữa handshake | LINK hoặc SERVER | firewall chặn, server quá tải |
| `-0x7F00` / `ESP_ERR_NO_MEM` | INIT | hết heap — giảm content len, giảm số kết nối đồng thời |
| `-0x6C00` `SSL_HANDSHAKE_FAILURE` | PROTO | không khớp phiên bản TLS hoặc cipher suite |

Khi báo lỗi TLS ra log, luôn in cả `esp_tls_last_esp_err`, mã mbedTLS, và flags verify
(`esp_tls_get_and_clear_last_error`) — nếu không thì mọi lỗi trông như nhau.
