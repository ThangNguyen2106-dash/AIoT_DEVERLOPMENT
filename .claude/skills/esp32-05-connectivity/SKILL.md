---
name: esp32-05-connectivity
description: Kết nối mạng cho ESP32 — Wi-Fi station/AP/provisioning, BLE GATT, ESP-NOW, Ethernet, modem cellular PPPoS, TCP/UDP, MQTT, HTTP/HTTPS, WebSocket, TLS, SNTP. Kèm khung bắt buộc về connection state machine, reconnect có backoff, timeout, offline mode, buffering và phân loại lỗi mạng. Dùng khi firmware cần lên mạng, mất kết nối không tự phục hồi, hoặc mất dữ liệu lúc rớt mạng.
---

# 05 — Connectivity

## Điều kiện vào
Chưa rõ chip / framework / phiên bản IDF → chạy Context Gate ở `esp32-firmware` trước.
Cổng an toàn và ngân sách context của `esp32-firmware` áp dụng nguyên vẹn tại đây.

Mọi thứ đi qua radio, dây mạng hoặc socket: chọn link, dựng transport, và **vòng đời kết nối**.
Phần khó không nằm ở lệnh connect — nằm ở chỗ kết nối chắc chắn sẽ hỏng, và firmware phải sống sót.

## Luật số 1: cấm code chỉ có happy path

Không bao giờ sinh code mạng theo dạng `connect() → send() → xong`. Mọi đường mạng viết ra
phải trả lời được đủ 7 câu hỏi dưới đây; thiếu câu nào thì code chưa xong:

1. Khởi tạo hỏng thì sao? (sai config, hết heap, chưa init NVS, chip không có ngoại vi đó)
2. Đang chạy mà rớt thì sao? Ai phát hiện, sau bao lâu, và hệ thống về trạng thái nào?
3. Kết nối lại theo lịch nào? Backoff bao nhiêu, trần bao nhiêu, có jitter chưa, thử mãi hay đổi chiến lược?
4. Mọi thao tác chờ có timeout chưa? Con số đó lấy từ đâu ra?
5. Trong lúc mất mạng, dữ liệu sinh ra đi đâu? Hàng đợi bao lớn, đầy thì bỏ cái nào?
6. Xác thực hỏng thì có retry không? (câu trả lời gần như luôn là **không retry nhanh**)
7. Thiết bị làm được gì khi hoàn toàn không có mạng? Offline phải là trạng thái hợp lệ, không phải lỗi.

## Luật số 2: phân loại lỗi trước khi xử lý lỗi

Gộp mọi lỗi thành "connect fail rồi thử lại" là nguyên nhân số một khiến thiết bị kẹt trong
vòng retry vô tận, tự khoá tài khoản trên broker, hoặc đốt hết pin trong một đêm. Mỗi lỗi mạng
phải được quy về đúng **một** trong 5 lớp sau, mỗi lớp có chính sách riêng:

| Lớp lỗi | Bản chất | Ví dụ | Chính sách bắt buộc |
|---|---|---|---|
| **INIT_FAIL** | Sai cấu hình / thiếu tài nguyên / sai phần cứng. Là lỗi lập trình hoặc lỗi build. | `esp_wifi_init` trả `ESP_ERR_NO_MEM`, chưa `nvs_flash_init()`, chip không có Ethernet MAC, sai chân SPI của W5500 | **Không retry.** Log ERROR đủ chi tiết, báo lên tầng trên, vào trạng thái FAULT. Retry không bao giờ sửa được sai config. |
| **LINK_FAIL** (tạm thời) | Môi trường: không thấy AP, mất sóng, DNS hỏng, socket timeout, rút cáp, modem mất sóng | reason 201 `NO_AP_FOUND`, `ESP_ERR_TIMEOUT`, `ECONNREFUSED`, `EHOSTUNREACH` | **Retry có backoff + jitter + trần.** Đây là lớp duy nhất được retry tương đối nhanh. |
| **AUTH_FAIL** | Danh tính sai hoặc bị từ chối | reason 202 `AUTH_FAIL`, MQTT CONNACK 0x04/0x05, HTTP 401/403, server từ chối client cert | **Không retry nhanh.** Tối thiểu 5–15 phút/lần, đếm số lần, báo trạng thái ra ngoài (LED/log/UI). Sai mật khẩu mà retry mỗi giây chỉ dẫn tới bị chặn MAC. |
| **PROTO_FAIL** | Hai bên không hiểu nhau, hoặc payload sai | MQTT CONNACK 0x01 (sai version), WebSocket handshake không trả 101, HTTP 400/404/415, response không parse được | **Không retry cùng một payload.** Đây là bug firmware hoặc lệch hợp đồng API. Bỏ message, tăng counter, log nguyên nhân. |
| **SERVER_FAIL** | Phía kia còn sống nhưng đang hỏng hoặc quá tải | HTTP 500/502/503/429, broker đóng kết nối ngay sau khi nhận | **Backoff dài hơn LINK_FAIL**, tôn trọng `Retry-After` nếu có, **giữ nguyên buffer** vì dữ liệu vẫn còn giá trị. |

Enum dùng chung và bảng ánh xạ mã lỗi thật (Wi-Fi reason, errno, CONNACK, HTTP status,
mbedTLS) → `references/connection-contract.md`.

## Quy trình 9 bước cho mọi đường kết nối

Chạy tuần tự, không nhảy vào code ở bước 1.

| # | Bước | Đầu ra |
|---|---|---|
| 1 | **Chọn link** | Wi-Fi / Ethernet / cellular / ESP-NOW / BLE — kèm lý do và phương án dự phòng |
| 2 | **Chọn transport** | MQTT / HTTPS / WebSocket / TCP / UDP — kèm chiều dữ liệu, tần suất, kích thước gói |
| 3 | **State machine** | liệt kê state, sự kiện, chuyển trạng thái; ai giữ state, ai đọc được |
| 4 | **Timeout** | bảng timeout cho từng thao tác, mỗi giá trị có lý do |
| 5 | **Retry & backoff** | chính sách theo từng lớp lỗi ở bảng trên |
| 6 | **Offline mode** | thiết bị làm gì khi không mạng; cái gì vẫn phải chạy (an toàn, điều khiển cục bộ) |
| 7 | **Buffering** | nơi lưu, dung lượng, chính sách khi đầy, có cần bền vững qua reboot không |
| 8 | **Bảo mật** | TLS hay không, xác thực thế nào, bí mật nằm ở đâu (→ `esp32-10-security`) |
| 9 | **Quan sát** | counter và log để chẩn đoán từ xa: số lần rớt, lý do cuối, thời gian online, số message bị bỏ |

## Hợp đồng bắt buộc của một component kết nối

```c
/* net_link.h — mọi link (wifi/eth/ppp) và mọi transport (mqtt/http/ws) đều theo khuôn này */
typedef enum {
    NET_STATE_UNINIT = 0,   /* chưa init                                        */
    NET_STATE_FAULT,        /* INIT_FAIL, hoặc AUTH_FAIL kéo dài — cần can thiệp */
    NET_STATE_IDLE,         /* đã init, chưa bật, hoặc chủ động offline          */
    NET_STATE_CONNECTING,   /* đang thử — có deadline, không vô hạn              */
    NET_STATE_CONNECTED,    /* có link nhưng CHƯA chắc dùng được (chưa IP/chưa session) */
    NET_STATE_READY,        /* dùng được thật: có IP + session + (nếu cần) giờ đã đồng bộ */
    NET_STATE_BACKOFF,      /* đang chờ tới lần thử tiếp theo                    */
} net_state_t;

esp_err_t   net_start(void);                 /* không blocking; kết quả báo qua event */
esp_err_t   net_stop(void);                  /* giải phóng sạch, kể cả khi đang CONNECTING */
net_state_t net_get_state(void);
bool        net_is_ready(void);              /* tầng ứng dụng CHỈ được hỏi hàm này */
esp_err_t   net_get_stats(net_stats_t *out); /* uptime, số lần rớt, lý do cuối, số message bỏ */
```

Quy ước không được phá:
- **Chỉ `READY` mới được gửi dữ liệu.** "Có Wi-Fi" không đồng nghĩa "có IP"; "có IP" không
  đồng nghĩa "có session MQTT"; "có session" không đồng nghĩa "giờ hệ thống đủ đúng để bắt tay TLS".
- Callback/event handler của stack mạng **không** chứa logic nghiệp vụ và **không** blocking.
  Nhận event → đổi state → đẩy queue → thoát.
- Tầng ứng dụng không bao giờ gọi thẳng `esp_mqtt_client_publish()`. Nó đẩy vào queue có giới
  hạn; task mạng là nơi duy nhất chạm socket.
- Không có `while (!connected) vTaskDelay(...)` ở bất kỳ đâu trong code ứng dụng.
- Không `portMAX_DELAY` trên bất kỳ thao tác mạng nào.

## Định tuyến reference — đọc đúng file đang cần, không đọc cả thư mục

| Cần làm | Reference |
|---|---|
| State machine, backoff, timeout, retry, offline, buffering, bảng mã lỗi → lớp lỗi | `references/connection-contract.md` |
| Wi-Fi station/AP, reason code, provisioning, quét, RSSI | `references/wifi.md` |
| Ethernet: PHY nội, W5500/ENC28J60 qua SPI, phát hiện rút cáp | `references/ethernet.md` |
| Modem cellular ngoài: `esp_modem`, PPPoS, lệnh AT, SIM, tín hiệu, dung lượng data | `references/cellular.md` |
| BLE GATT, ESP-NOW, chọn công nghệ không dây | `references/ble-espnow.md` |
| MQTT: client id, LWT, QoS, CONNACK, buffer offline, message phân mảnh | `references/mqtt.md` |
| HTTP/HTTPS client và server, status code, stream, rò socket | `references/http.md` |
| WebSocket client: handshake, ping/pong, phát hiện kết nối chết câm | `references/websocket.md` |
| TCP/UDP thuần: socket option, errno, khung parser, half-open | `references/tcp-udp.md` |
| TLS: CA bundle, SNTP, mTLS, SNI, stack/heap, CA hết hạn | `references/tls.md` |
| Rà soát trước khi coi là xong | `checklists/connectivity-checklist.md` |

## Mẫu báo cáo trước khi viết code

```
## Link và transport
Link: <Wi-Fi STA / Ethernet W5500 / SIM7600 PPPoS>   — dự phòng: <có/không, cái gì>
Transport: <MQTT over TLS 8883>                       — chiều: <up / down / cả hai>
Tần suất: <1 mẫu/10 s>   Kích thước: <~180 byte JSON>

## State machine
<state + sự kiện + chuyển trạng thái; nêu rõ điều kiện đủ để vào READY>

## Bảng timeout
| Thao tác | Timeout | Vì sao |
|---|---|---|
| chờ GOT_IP | 15 s | DHCP chậm nhất quan sát được ~8 s |
| TLS handshake | 10 s | ... |
| publish QoS1 chờ PUBACK | 5 s | ... |

## Chính sách theo lớp lỗi
| Lớp | Nguồn lỗi cụ thể ở dự án này | Hành động |
|---|---|---|
| INIT_FAIL | ... | vào FAULT, không retry |
| LINK_FAIL | ... | backoff 1→2→4→…→60 s, jitter ±20% |
| AUTH_FAIL | ... | thử lại sau 10 phút, báo LED đỏ |
| PROTO_FAIL | ... | bỏ message, đếm, log |
| SERVER_FAIL | ... | backoff tới 5 phút, giữ buffer |

## Offline mode
<thiết bị vẫn làm gì; cái gì bị dừng; cái gì tuyệt đối không được dừng>

## Buffering
Nơi lưu: <ring buffer RAM 64 bản ghi / NVS / file LittleFS>
Đầy thì: <bỏ bản ghi cũ nhất>    Bền qua reboot: <có / không>

## Quan sát
<danh sách counter và log sẽ có để chẩn đoán từ xa>
```

## Không thuộc scope

- Chọn chip có Wi-Fi/BLE/Ethernet hay không, gán chân cho PHY/modem → `esp32-02-hardware-analysis`
- Task nào chạy mạng, priority, stack, queue giữa các task → `esp32-03-firmware-architecture`
- Driver UART cho modem ở mức byte, driver SPI cho W5500 → `esp32-04-driver-development`
- Nội dung nghiệp vụ của message, schema, lưu NVS/file → `esp32-06-application-development`
- Crash/treo đã xảy ra trên board thật → `esp32-07-debugging`
- Lưu bí mật an toàn, chống tấn công qua đầu vào mạng, Secure Boot → `esp32-10-security`
- Tắt radio tiết kiệm pin, chu kỳ kết nối-gửi-ngủ → `esp32-11-power-management`
- Tải firmware qua HTTPS và cài đặt, rollback → `esp32-12-release`

## Đầu ra
Component kết nối hoàn chỉnh: state machine rõ ràng, bảng timeout, chính sách theo 5 lớp lỗi,
hàng đợi offline có giới hạn, counter chẩn đoán — kèm báo cáo theo mẫu trên.
