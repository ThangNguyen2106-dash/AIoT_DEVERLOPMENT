# Hợp đồng kết nối — state, lỗi, timeout, retry, offline, buffering

File này là phần lõi của skill 05. Mọi reference khác chỉ nói về đặc thù của một giao thức;
khung xử lý vòng đời thì lấy ở đây.

## 1. Phân lớp lỗi

```c
typedef enum {
    NET_OK = 0,
    NET_FAIL_INIT,    /* sai config / thiếu tài nguyên / sai phần cứng — KHÔNG retry   */
    NET_FAIL_LINK,    /* tạm thời: không có AP, timeout, DNS, cáp rút — retry + backoff */
    NET_FAIL_AUTH,    /* danh tính bị từ chối — retry rất chậm, cần can thiệp          */
    NET_FAIL_PROTO,   /* hai bên không hiểu nhau / payload sai — KHÔNG retry cùng payload */
    NET_FAIL_SERVER,  /* server quá tải hoặc hỏng — backoff dài, giữ buffer            */
} net_fail_t;

typedef struct {
    net_fail_t  cls;
    int         code;        /* mã gốc: wifi reason, errno, CONNACK, HTTP status, -mbedtls */
    const char *where;       /* "wifi", "dns", "tls", "mqtt_connack", "http" */
} net_err_t;
```

Luôn giữ lại `code` gốc. Chỉ giữ lớp lỗi là mất khả năng chẩn đoán từ xa.

### Ánh xạ nguồn lỗi → lớp

**Wi-Fi `wifi_event_sta_disconnected_t.reason`**

| reason | Lớp | Ghi chú |
|---|---|---|
| 201 `NO_AP_FOUND` | LINK | sai SSID, AP 5 GHz, ngoài vùng phủ |
| 202 `AUTH_FAIL`, 204 `HANDSHAKE_TIMEOUT`, 15 `4WAY_HANDSHAKE_TIMEOUT` | AUTH | gần như luôn là sai mật khẩu |
| 203 `ASSOC_FAIL`, 205 `CONNECTION_FAIL` | LINK | AP từ chối hoặc quá tải |
| 2 `AUTH_EXPIRE`, 8 `ASSOC_LEAVE`, 4 `ASSOC_EXPIRE` | LINK | AP chủ động ngắt |
| 39, 24, 200 `BEACON_TIMEOUT` | LINK | sóng yếu, thiết bị đi ra xa |

Lưu ý: chuỗi 202 lặp lại nhiều lần mới nên kết luận AUTH — một lần 202 lẻ có thể do nhiễu.
Khuyến nghị: 3 lần 202 liên tiếp → coi là `NET_FAIL_AUTH`.

**errno của socket (`lwip`)**

| errno | Lớp | Nghĩa |
|---|---|---|
| `ECONNREFUSED` | LINK | tới được host, không ai nghe cổng đó (hoặc server đang restart) |
| `EHOSTUNREACH`, `ENETUNREACH` | LINK | routing/hết sóng/chưa có IP |
| `ETIMEDOUT`, `EAGAIN` trên socket có timeout | LINK | không phản hồi |
| `ECONNRESET`, `EPIPE` | LINK | phía kia đóng đột ngột |
| `EINVAL`, `EBADF`, `EAFNOSUPPORT` | INIT | bug code: dùng fd đã đóng, sai tham số |
| `ENOMEM`, `ENOBUFS` | INIT | hết heap / hết socket — xem lại giới hạn `LWIP_MAX_SOCKETS` |

**DNS**: `getaddrinfo` fail → LINK (trừ khi hostname hard-code sai chính tả → PROTO).
Cách phân biệt nhanh khi gỡ lỗi: thử kết nối bằng IP thuần; chạy được nghĩa là lỗi DNS.

**MQTT CONNACK**

| code | Lớp |
|---|---|
| 0x01 sai protocol version | PROTO |
| 0x02 client id bị từ chối | PROTO |
| 0x03 server unavailable | SERVER |
| 0x04 sai user/pass | AUTH |
| 0x05 not authorized | AUTH |

**HTTP status**

| status | Lớp |
|---|---|
| 2xx | OK |
| 301/302/307 | tuỳ: có xử lý redirect thì follow, không thì PROTO |
| 400, 404, 405, 409, 415, 422 | PROTO |
| 401, 403 | AUTH |
| 408, 425 | LINK |
| 429 | SERVER (đọc `Retry-After`) |
| 5xx | SERVER |

**TLS (mbedTLS, giá trị âm)**

| lỗi | Lớp | Nguyên nhân thật |
|---|---|---|
| `X509_CERT_VERIFY_FAILED` (-0x2700) | AUTH | thiếu CA, giờ hệ thống sai, cert hết hạn |
| `SSL_FATAL_ALERT_MESSAGE` với alert 42/48 | AUTH | server từ chối client cert (mTLS) |
| `SSL_CONN_EOF` / reset giữa handshake | LINK hoặc SERVER | mạng chặn, hoặc server quá tải |
| `SSL_ALLOC_FAILED`, `-0x7F00` | INIT | hết heap — giảm `MBEDTLS_SSL_IN_CONTENT_LEN` |

Giờ hệ thống sai là nguyên nhân số một của lỗi verify. Kiểm tra SNTP trước khi kết luận cert hỏng.

## 2. State machine chuẩn

```
UNINIT ──init lỗi──────────────────────────────► FAULT
   │ init ok
   ▼
  IDLE ──start()──► CONNECTING ──link up + IP──► CONNECTED ──session ok──► READY
                        │  ▲                          │                      │
                  timeout/LINK_FAIL                  rớt                    rớt
                        ▼  │                          ▼                      ▼
                     BACKOFF ◄────────────────────────┴──────────────────────┘
                        │
              AUTH_FAIL x N liên tiếp
                        ▼
                      FAULT  (chờ người dùng: đổi cấu hình / provisioning lại / reboot)
```

Bất biến phải giữ:
- Chuyển vào `READY` chỉ khi **tất cả** điều kiện đủ đã thoả (IP + session + giờ nếu cần TLS).
- Ra khỏi `READY` phải thông báo cho tầng ứng dụng ngay (event/flag), không để ứng dụng tự phát hiện bằng lỗi gửi.
- `BACKOFF` có thể bị cắt sớm bởi sự kiện bên ngoài (cắm lại cáp, nhấn nút, có scan thấy AP) — gọi là *fast path*, cho phép; nhưng không được reset bộ đếm backoff nếu lần thử ngay sau đó lại hỏng.
- `FAULT` không tự thoát bằng retry. Chỉ thoát khi có can thiệp (cấu hình mới, nút reset, hoặc reboot có chủ đích).

Cài đặt gợi ý: một task mạng duy nhất, chạy vòng lặp lấy từ queue với timeout = thời gian còn
lại của backoff. Event handler của Wi-Fi/MQTT chỉ `xQueueSend` vào queue này.

```c
static void net_task(void *arg)
{
    net_evt_t evt;
    for (;;) {
        TickType_t wait = net_next_deadline_ticks();   /* pdMS_TO_TICKS(...) hoặc 0 */
        if (xQueueReceive(q, &evt, wait) == pdTRUE) {
            net_handle_event(&evt);                     /* đổi state, không blocking lâu */
        } else {
            net_handle_timeout();                       /* hết backoff → thử lại; hết deadline → huỷ */
        }
    }
}
```

## 3. Timeout

Nguyên tắc: **mọi** thao tác chờ đều có deadline, và deadline phải có lý do viết ra được.

| Thao tác | Giá trị khởi điểm | Ghi chú |
|---|---|---|
| Wi-Fi associate → GOT_IP | 15 s | DHCP chậm; quá hạn thì `esp_wifi_disconnect()` rồi backoff |
| Ethernet link up → GOT_IP | 10 s | |
| PPP dial (cellular) | 60–90 s | mạng di động thật sự chậm, đừng đặt 10 s |
| DNS resolve | 5 s | `CONFIG_LWIP_DNS_TIMEOUT` |
| TCP connect | 10 s | đặt bằng socket non-blocking + `select`, xem `tcp-udp.md` |
| TLS handshake | 10–15 s | cellular thì 30 s |
| HTTP request/response | 10 s (`timeout_ms`) | luôn đặt, mặc định có thể treo rất lâu |
| MQTT keepalive | 30–60 s | phát hiện chết câm sau ~1.5× keepalive |
| MQTT publish QoS1 chờ PUBACK | 5–10 s | quá hạn → coi như chưa gửi, giữ trong buffer |
| WebSocket ping → pong | 10 s | không có pong → đóng và reconnect |
| Toàn bộ chu kỳ CONNECTING | 60 s | trần tuyệt đối; quá thì huỷ sạch và về BACKOFF |

Deadline tổng của `CONNECTING` là bắt buộc: có trường hợp từng bước đều chưa timeout nhưng
tổng thể kẹt vô hạn (ví dụ TLS renegotiation lặp).

## 4. Retry và backoff

```c
typedef struct {
    uint32_t base_ms;      /* 1000  */
    uint32_t max_ms;       /* 60000 */
    uint8_t  jitter_pct;   /* 20    */
    uint8_t  attempt;      /* tăng mỗi lần hỏng, về 0 khi vào READY ổn định */
} backoff_t;

static uint32_t backoff_next(backoff_t *b)
{
    uint32_t d = b->base_ms;
    for (uint8_t i = 0; i < b->attempt && d < b->max_ms; i++) d <<= 1;
    if (d > b->max_ms) d = b->max_ms;

    uint32_t j = (uint64_t)d * b->jitter_pct / 100;
    d = d - j + (esp_random() % (2 * j + 1));      /* ±jitter_pct% */

    if (b->attempt < 30) b->attempt++;             /* chặn tràn */
    return d;
}
```

Quy tắc:
- **Jitter là bắt buộc.** Cả trăm thiết bị mất điện rồi bật lại cùng lúc sẽ đồng loạt đập vào
  broker theo đúng nhịp nếu không có jitter; server sập vì chính thiết bị của mình.
- **Trần là bắt buộc.** Backoff không trần sẽ thành "thử lại sau 9 giờ" và thiết bị coi như chết.
- Reset `attempt` chỉ khi kết nối đã **ổn định** (ví dụ ở `READY` liên tục ≥ 30 s). Reset ngay
  khi vừa connect sẽ tạo vòng lặp connect–rớt–connect với tần suất tối đa.
- Backoff theo lớp lỗi:

| Lớp | base | max | Ghi chú |
|---|---|---|---|
| LINK | 1 s | 60 s | |
| SERVER | 5 s | 300 s | ưu tiên `Retry-After` |
| AUTH | 300 s | 900 s | và báo trạng thái ra ngoài |
| PROTO | — | — | không retry; sửa code hoặc bỏ message |
| INIT | — | — | không retry; vào FAULT |

- **Đổi chiến lược sau N lần hỏng**, đừng thử mãi một cách: sau 10 lần LINK_FAIL liên tiếp →
  quét lại kênh/AP khác, hoặc chuyển sang link dự phòng (cellular), hoặc hạ xuống chế độ
  offline sâu và chỉ thử lại mỗi 15 phút để giữ pin.
- Reboot như cách "chữa mạng" chỉ được dùng khi có lý do rõ (rò tài nguyên đã xác nhận) và
  phải có counter + trần số lần; reboot vòng lặp là mất thiết bị ngoài hiện trường.

## 5. Offline mode

Offline không phải lỗi. Là một trạng thái vận hành hợp lệ, phải thiết kế từ đầu:

- Chức năng an toàn và điều khiển cục bộ **không được** phụ thuộc mạng. Không có mạng thì
  bơm vẫn phải tắt đúng ngưỡng, cửa vẫn phải đóng đúng lịch.
- Cơ cấu chấp hành phải có fail-safe theo thời gian: lệnh từ cloud có thời hạn hiệu lực; mất
  mạng quá X phút thì về trạng thái an toàn, không giữ lệnh cũ vô hạn.
- Đo đạc vẫn chạy và vẫn ghi timestamp; dữ liệu vào buffer.
- Giờ: nếu chưa từng đồng bộ SNTP, đánh dấu bản ghi là "thời gian tương đối từ lúc boot" để
  server sửa lại sau, đừng gửi năm 1970 như thời gian thật.
- Có chỉ báo ra ngoài (LED, màn hình, log) để người vận hành biết đang offline, không đoán mò.

## 6. Message buffering

```c
typedef struct {
    uint64_t ts_ms;        /* thời điểm sinh, không phải thời điểm gửi */
    uint16_t len;
    uint8_t  payload[MAX_MSG];
} net_msg_t;
```

Quyết định phải nêu rõ trong thiết kế:

| Câu hỏi | Lựa chọn và hệ quả |
|---|---|
| Lưu ở đâu | RAM ring buffer (nhanh, mất khi reboot) / NVS (ít ghi, hạn chế dung lượng) / file LittleFS hoặc SD (nhiều, phải quản lý wear) |
| Bao lớn | Tính ra byte: `tần suất × kích thước × thời gian offline chấp nhận được`. Ví dụ 1 bản ghi/10 s × 200 B × 1 giờ = 72 KB → không nằm vừa RAM, phải ra flash. |
| Đầy thì bỏ gì | Telemetry: bỏ **cũ nhất** (dữ liệu mới có giá trị hơn). Sự kiện/alarm: bỏ **mới nhất** và giữ cũ, hoặc dùng hàng đợi riêng không bao giờ bị bỏ. |
| Có bền qua reboot | Cảnh báo và dữ liệu tính tiền: có. Telemetry môi trường: thường không cần. |
| Gửi lại thế nào | Khi vào READY, xả dần **có tiết chế** (ví dụ 10 message/giây), không xả một phát cả nghìn bản ghi — broker sẽ chặn và TLS buffer sẽ nổ. |
| Trùng lặp | Gửi lại có thể tạo bản ghi trùng. Mỗi message có id tăng dần để server khử trùng (idempotency). |

Cấm tuyệt đối: hàng đợi cấp phát không giới hạn (`malloc` mỗi message, không có trần).
Mất mạng nửa ngày là hết heap, rồi reset, rồi mất luôn dữ liệu.

## 7. Counter chẩn đoán tối thiểu

Không có mấy con số này thì không thể gỡ lỗi thiết bị đang ở ngoài hiện trường:

```c
typedef struct {
    uint32_t connect_attempts;
    uint32_t connect_success;
    uint32_t disconnects;
    net_err_t last_err;        /* lớp + mã gốc + chỗ xảy ra */
    uint64_t uptime_online_ms;
    uint32_t msg_queued, msg_sent, msg_dropped;
    int8_t   last_rssi;        /* hoặc CSQ với cellular */
} net_stats_t;
```

Đưa bộ này vào telemetry định kỳ, và vào log khi rớt kết nối. `msg_dropped > 0` là dấu hiệu
buffer đang bị tràn — hoặc mạng kém hơn dự tính, hoặc buffer quá nhỏ.

## 8. Những lỗi cài đặt hay gặp

- Gọi `esp_wifi_connect()` ngay trong event handler `STA_DISCONNECTED` không có delay → bão
  reconnect, log tràn, không bao giờ vào được AP.
- Coi `WIFI_EVENT_STA_CONNECTED` là "có mạng" → mọi socket fail vì chưa có IP.
- Tạo lại client MQTT/HTTP mỗi lần rớt mà không `destroy` cái cũ → rò heap, chết sau vài giờ.
- Kiểm tra `err == ESP_OK` mà bỏ qua HTTP status → xử lý trang lỗi 500 như dữ liệu hợp lệ.
- Blocking trong callback của stack mạng → deadlock hoặc watchdog.
- Dùng cùng một `client_id`/serial cho mọi thiết bị → chúng đá nhau khỏi broker vô tận.
- Retry AUTH_FAIL mỗi giây → bị router hoặc cloud chặn.
- Không có deadline tổng cho `CONNECTING` → kẹt mãi ở trạng thái "đang kết nối".

## Ánh xạ với từ vựng lỗi của driver

`esp32-04-driver-development` phân loại lỗi theo **mã `esp_err_t`** (lỗi tham số / timeout /
CRC / thiết bị không phản hồi). File này phân loại theo **5 lớp ngữ nghĩa**. Khi một đường dữ
liệu đi qua cả hai skill — điển hình là modem cellular (driver UART ở 04 + PPPoS ở 05) hoặc
Ethernet W5500 (driver SPI + link ở 05) — dùng bảng này để quy đổi, đừng dựng hai hệ thống lỗi
song song:

| Driver (04) trả về | Lớp lỗi mạng (05) | Lý do |
|---|---|---|
| `ESP_ERR_INVALID_ARG`, sai chân, sai cấu hình bus | **INIT_FAIL** | lỗi lập trình/build, retry không sửa được |
| `ESP_ERR_NO_MEM` lúc init | **INIT_FAIL** | như trên |
| `ESP_ERR_TIMEOUT` trên bus, thiết bị không phản hồi | **LINK_FAIL** | môi trường/phần cứng tạm thời |
| CRC sai, khung hỏng, byte rác | **PROTO_FAIL** | hai bên không hiểu nhau |
| Modem trả `ERROR` cho lệnh AT xác thực, SIM bị khoá | **AUTH_FAIL** | danh tính bị từ chối — không retry nhanh |
| Modem báo mất sóng, không đăng ký được mạng | **LINK_FAIL** | môi trường |
| Driver vào state `ERROR` sau khi hết cách recovery | lớp tương ứng, **leo thang lên tầng mạng** | driver đã thử xong phần của nó |

Nguyên tắc ranh giới: **driver retry ở mức giao dịch** (số lần cố định, khai báo trong config);
**tầng mạng retry ở mức kết nối** (backoff, jitter, trần). Hai tầng không được cùng retry một
việc — retry lồng nhau làm thời gian chờ thật nhân lên và rất khó suy luận.
