# Modem cellular ngoài (2G/4G/NB-IoT/LTE-M)

Dùng khi project có module ngoài: SIM800/SIM7600/SIM7070, EC200/BG96, A7670… ESP32 **không có
modem tích hợp**; toàn bộ phần này là UART + `esp_modem` + PPPoS.

## Trước khi viết một dòng code — checklist phần cứng

Bỏ qua mấy mục này thì mọi lỗi sau đó đều bị chẩn đoán nhầm thành lỗi phần mềm:

| Mục | Yêu cầu | Triệu chứng khi sai |
|---|---|---|
| Dòng đỉnh | 2 A xung ở 2G, ~0.5–1 A ở LTE; cần tụ ≥ 1000 µF sát module | ESP32 brownout/reset đúng lúc modem phát |
| Mức logic | Nhiều module là 1.8 V — phải level shift | modem "không phản hồi AT" dù đã cấp nguồn |
| Chân PWRKEY / RESET | Cần chuỗi bật đúng (giữ mức X ms) | modem không bao giờ khởi động |
| SIM | PIN đã tắt, còn data, đúng băng tần khu vực | đăng ký mạng mãi không xong |
| Ăng-ten | Đúng băng, nối chắc | CSQ = 99 (không đo được) |

Chi tiết dòng điện và chân → `esp32-02-hardware-analysis`; driver UART mức byte →
`esp32-04-driver-development`.

## Dựng PPPoS bằng esp_modem

```c
esp_modem_dte_config_t dte = ESP_MODEM_DTE_DEFAULT_CONFIG();
dte.uart_config.tx_io_num = PIN_TX;
dte.uart_config.rx_io_num = PIN_RX;
dte.uart_config.rts_io_num = PIN_RTS;            /* bật flow control nếu có dây */
dte.uart_config.cts_io_num = PIN_CTS;
dte.uart_config.flow_control = ESP_MODEM_FLOW_CONTROL_HW;
dte.uart_config.rx_buffer_size = 4096;           /* nhỏ quá là mất byte, PPP hỏng */

esp_modem_dce_config_t dce = ESP_MODEM_DCE_DEFAULT_CONFIG("internet");  /* APN */
esp_netif_config_t ncfg = ESP_NETIF_DEFAULT_PPP();
s_netif = esp_netif_new(&ncfg);
s_dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7600, &dte, &dce, s_netif);
if (!s_dce) return ESP_ERR_NO_MEM;               /* INIT_FAIL */
```

Trình tự bắt buộc **trước** khi vào chế độ data — mỗi bước là một cổng kiểm tra riêng, đừng gộp:

```c
/* 1. Modem có sống không? */
if (esp_modem_sync(s_dce) != ESP_OK)            return NET_FAIL_INIT;   /* dây/nguồn/PWRKEY */

/* 2. SIM có sẵn sàng không? */
if (esp_modem_read_pin(s_dce, &pin_ok) != ESP_OK || !pin_ok)
                                                 return NET_FAIL_AUTH;  /* SIM khoá PIN/không có SIM */

/* 3. Có sóng không? */
int rssi, ber;
esp_modem_get_signal_quality(s_dce, &rssi, &ber);
if (rssi == 99 || rssi < 5)                      return NET_FAIL_LINK;  /* ăng-ten/vùng phủ */

/* 4. Đã đăng ký mạng chưa? (AT+CREG/CGREG) — chờ có deadline, thường 60–90 s */
if (!wait_registered(60000))                     return NET_FAIL_LINK;

/* 5. Vào chế độ data */
if (esp_modem_set_mode(s_dce, ESP_MODEM_MODE_DATA) != ESP_OK)
                                                 return NET_FAIL_LINK;
/* 6. Chờ IP_EVENT_PPP_GOT_IP — deadline 60 s; quá hạn: APN sai hoặc hết dung lượng data */
```

Phân biệt đúng chỗ hỏng là toàn bộ giá trị của đoạn trên: "không lên mạng được" có thể là dây
UART, SIM, sóng, đăng ký mạng, APN, hay tài khoản hết tiền — năm nguyên nhân khác hẳn nhau.

## Ánh xạ lỗi cellular → lớp lỗi

| Hiện tượng | Lớp | Xử lý |
|---|---|---|
| `esp_modem_sync` fail (không có "OK") | INIT | kiểm tra baud, TX/RX chéo, PWRKEY, nguồn. Không retry vô tận. |
| Không có SIM / SIM khoá PIN | AUTH | vào FAULT, báo ra ngoài. Retry không giải quyết được. |
| CSQ = 99 hoặc quá thấp | LINK | backoff, thử lại; sau N lần thì cân nhắc reset modem bằng PWRKEY |
| Không đăng ký được mạng (CREG 0/2 kéo dài) | LINK | backoff dài (30–120 s), roaming/băng tần |
| PPP lên rồi rớt sau vài giây | LINK/SERVER | thường do APN sai, hoặc nhà mạng cắt vì hết data |
| `AT+CGDCONT` bị từ chối | PROTO | sai cú pháp/APN không tồn tại — bug cấu hình |
| Modem treo, không nhận AT nữa | LINK → escalate | reset cứng bằng PWRKEY/RESET, có đếm số lần |

Đặc thù: **reset cứng modem là công cụ recovery hợp lệ** ở đây (khác với reboot ESP32). Nhưng
phải có bậc thang: escape sequence `+++` → `AT+CFUN=1,1` → xung PWRKEY → cắt nguồn module.
Mỗi bậc có counter và trần.

## Timeout đặc thù

Mạng di động chậm hơn Wi-Fi một bậc. Dùng thẳng timeout của Wi-Fi sẽ hỏng:

| Thao tác | Timeout |
|---|---|
| lệnh AT thường | 1–5 s |
| `AT+CFUN`, `AT+COPS=?` (quét mạng) | 60–180 s |
| đăng ký mạng | 60–90 s |
| PPP dial → GOT_IP | 60 s |
| TLS handshake qua PPP | 30 s |
| HTTP request | 30–60 s |

## Dung lượng data — ràng buộc thiết kế, không phải chi tiết vận hành

SIM IoT thường tính theo MB/tháng. Mỗi lần dựng lại TLS tốn ~5–8 KB chỉ riêng handshake. Một
thiết bị reconnect 1 phút/lần sẽ đốt vài trăm MB/tháng mà chẳng gửi được dữ liệu nào có ích.

Hệ quả bắt buộc:
- Backoff trần phải **dài hơn** so với Wi-Fi: max 5–15 phút, không phải 60 giây.
- Ưu tiên MQTT giữ session dài hơn là HTTPS mỗi lần một request.
- Gom nhiều mẫu vào một message (batch) thay vì gửi từng mẫu.
- Bật TLS session resumption nếu broker hỗ trợ.
- Đếm số byte đã gửi/nhận và đưa vào telemetry — đây là counter quan trọng nhất của thiết bị cellular.

## Cùng tồn tại với Wi-Fi

Thường dùng cellular làm dự phòng cho Wi-Fi/Ethernet (hoặc ngược lại). Quy tắc chuyển link
giống phần "Ethernet + Wi-Fi cùng lúc" trong `ethernet.md`: đóng và dựng lại session, có trễ
chống nhảy qua lại, ghi log lý do. Thêm một điều: **cellular tốn tiền**, nên chuyển sang nó
phải là quyết định có điều kiện rõ ràng (mất link chính ≥ X phút), và phải tự động quay về khi
link chính hồi phục.

Khi không dùng, tắt hẳn modem (`AT+CFUN=0` hoặc cắt nguồn) — nó là tải lớn nhất trong hệ →
`esp32-11-power-management`.
