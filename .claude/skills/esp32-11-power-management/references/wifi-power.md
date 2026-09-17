# Điện của Wi-Fi, BLE và ESP-NOW

Với hầu hết thiết bị IoT chạy pin, **pha radio chiếm 80–95% năng lượng mỗi chu kỳ**. Lập bảng
ngân sách (`power-budget.md`) trước — nếu radio không phải khoản lớn nhất thì đừng tối ưu ở đây.

Mọi kỹ thuật trong file này đều đánh đổi **downlink latency** hoặc **reliability**. Không áp dụng
cái nào mà không ghi vào bảng 4 trục.

## Năng lượng của một lần kết nối Wi-Fi

| Pha | Thời gian điển hình | Ghi chú |
|---|---|---|
| Quét (scan) | 1–3 s | tốn nhất; quét hết kênh còn lâu hơn |
| Xác thực + kết hợp (auth/assoc) | 100–500 ms | WPA2 handshake |
| DHCP | 100 ms – 2 s | có thể timeout rất lâu khi mạng bận |
| DNS | 50–500 ms | mỗi lần resolve |
| TCP + TLS handshake | 0.5–3 s | TLS full handshake đắt nhất, cả CPU lẫn radio |
| MQTT CONNECT + publish | 100–300 ms | phần việc thật, thường nhỏ nhất |

Đọc bảng này ra một kết luận: **phần việc hữu ích chiếm rất ít; gần hết năng lượng nằm ở thủ tục
thiết lập.** Vì vậy hướng tối ưu đúng là *giảm số lần thiết lập* và *rút ngắn từng bước thiết lập*,
không phải giảm kích thước dữ liệu.

## Rút ngắn thiết lập — và cái giá của từng cách

| Kỹ thuật | Tiết kiệm | Đánh đổi phải công bố |
|---|---|---|
| Ghim BSSID + kênh vào RTC memory | bỏ 1–3 s quét | AP đổi kênh/roaming ⇒ lần đầu thất bại. **Bắt buộc có fallback quét đầy đủ** (`deep-sleep-state.md`) |
| IP tĩnh, bỏ DHCP | 0.1–2 s | xung đột IP nếu mạng đổi; không chạy được trên mạng lạ (khách hàng đổi router) |
| Ghim IP server, bỏ DNS | 0.05–0.5 s | server đổi IP ⇒ chết im. Cần fallback DNS và cập nhật được từ xa |
| TLS session resumption | 0.5–2 s | cần server hỗ trợ; ticket hết hạn phải fallback full handshake |
| MQTT session bền (`clean_session = false`) | bỏ resubscribe | broker giữ trạng thái; nếu broker restart thì phải phát hiện được |
| Giảm TX power (`esp_wifi_set_max_tx_power`) | vài chục mA khi TX | **giảm vùng phủ** — biên sóng yếu sẽ retry nhiều hơn và **tốn hơn**. Chỉ làm khi đo được RSSI dư dả |
| Gửi gộp nhiều mẫu | bỏ hẳn nhiều chu kỳ kết nối | dữ liệu trễ tới cả chu kỳ gộp; mất buffer nếu mất nguồn |

Giảm TX power là kỹ thuật hay bị dùng sai nhất: nó chỉ tiết kiệm khi sóng vốn đã khoẻ. Ở biên
vùng phủ, hạ công suất làm tăng retry ở tầng MAC → thời gian radio bật dài hơn → **tốn nhiều hơn
trước**, đồng thời giảm độ tin cậy. Phải đo RSSI và tỉ lệ retry trước/sau.

## Power save mode khi online

```c
esp_wifi_set_ps(WIFI_PS_NONE);        /* radio luôn bật — độ trễ thấp nhất, tốn nhất */
esp_wifi_set_ps(WIFI_PS_MIN_MODEM);   /* thức theo DTIM của AP */
esp_wifi_set_ps(WIFI_PS_MAX_MODEM);   /* thức theo listen interval của mình — tiết kiệm nhất */
```

| | `PS_NONE` | `PS_MIN_MODEM` | `PS_MAX_MODEM` |
|---|---|---|---|
| Dòng khi online | 40–100 mA | ~20 mA | ~2–20 mA (tuỳ interval) |
| Độ trễ nhận lệnh | < 10 ms | theo DTIM (100 ms – 1 s) | theo listen interval (tới vài giây) |
| Nguy cơ mất gói broadcast/multicast | thấp | trung bình | **cao** — AP không đệm mãi |
| Hợp cho | điều khiển tức thời | phần lớn IoT | chỉ nhận dữ liệu thưa |

**DTIM do AP quyết định, không phải thiết bị.** Nói với người dùng "đặt DTIM 10" là sai nếu họ
không kiểm soát AP. Thiết bị chỉ điều chỉnh được `listen_interval` ở `PS_MAX_MODEM`.

Đánh đổi quan trọng nhất phải nói rõ: ở `PS_MAX_MODEM` với listen interval dài, **lệnh từ server
có thể tới chậm hàng giây và gói broadcast có thể mất hẳn**. Nếu sản phẩm hứa "điều khiển tức
thời" thì đây là mất chức năng, không phải tinh chỉnh.

## Deep sleep có kết nối: mô hình nào?

| Mô hình | Power | Latency nhận lệnh | Reliability | Dùng khi |
|---|---|---|---|---|
| Online liên tục + modem sleep | cao | ms | cao | có nguồn, hoặc pin lớn |
| Online + auto light sleep | trung bình | ms – vài giây | trung bình (socket có thể đứt) | chu kỳ ngắn, cần nhận lệnh |
| Deep sleep, thức theo chu kỳ, kết nối mỗi lần | thấp | **bằng cả chu kỳ ngủ** | dữ liệu uplink tin cậy, downlink kém | cảm biến báo cáo định kỳ |
| Deep sleep dài + cửa sổ online ngắn định kỳ | rất thấp | bằng khoảng cách hai cửa sổ | trung bình | thiết bị cần nhận cấu hình thỉnh thoảng |

Với mô hình deep sleep, **downlink gần như không tồn tại**: server không gọi tới thiết bị đang
ngủ được. Mọi lệnh phải xếp hàng ở broker (MQTT retained / QoS 1 với session bền) và thiết bị lấy
về khi thức. Người dùng thường không tự nhận ra hệ quả này — phải nói thẳng: *"thiết bị sẽ nhận
lệnh chậm nhất là <chu kỳ> phút; nếu cần điều khiển tức thời thì mô hình này không dùng được."*

Cơ chế hàng đợi lệnh, QoS, LWT → `esp32-05-connectivity`.

## Trình tự pha radio tiết kiệm nhất

```
thức → đo cảm biến, chuẩn bị payload (radio VẪN TẮT)
     → esp_wifi_start + kết nối nhanh (BSSID ghim)
     → gửi, chờ ack với timeout NGẮN và xác định
     → lấy lệnh đang chờ (nếu có)
     → đóng session có trật tự
     → esp_wifi_stop
     → ngủ
```

Hai lỗi phổ biến làm hỏng ngân sách:
- Bật Wi-Fi ngay đầu `app_main` rồi mới đo cảm biến → radio bật thừa hàng giây mỗi chu kỳ.
- Không có timeout cứng cho toàn bộ pha radio → mạng hỏng thì thiết bị thức hàng phút, cạn pin
  trong vài ngày. **Phải có ngân sách thời gian tối đa cho pha radio**, hết thì bỏ cuộc, buffer
  dữ liệu, ngủ, thử lại chu kỳ sau với backoff.

```c
#define RADIO_BUDGET_MS  15000     /* hết thì bỏ cuộc, KHÔNG thử vô hạn */
```

Đây là điểm giao giữa power và connectivity: backoff khi thất bại vừa bảo vệ pin vừa bảo vệ
server. Thiết kế state machine reconnect → `esp32-05-connectivity`.

## Khi kết nối thất bại

Mỗi lần thất bại vẫn đốt gần đủ năng lượng như thành công. Chính sách phải rõ ràng:

| Tình huống | Chính sách |
|---|---|
| Thất bại 1–2 lần | thử lại ngay trong cùng cửa sổ (rẻ, đã bật radio rồi) |
| Thất bại hết ngân sách thời gian | tắt radio, buffer dữ liệu, ngủ tới chu kỳ sau |
| Thất bại nhiều chu kỳ liên tiếp | **giãn chu kỳ gửi** (backoff) để pin sống tới lúc mạng trở lại |
| Thất bại rất lâu | vào chế độ chỉ đo và lưu; thử mạng thưa hẳn; báo trạng thái khi nối lại được |

Backoff ở đây bảo vệ pin — nhưng cũng làm dữ liệu trễ hơn. Ghi vào bảng đánh đổi, và đếm
`connect_failures` để chẩn đoán được từ xa (`power-budget.md`).

## BLE

| Vai trò | Điện | Ghi chú |
|---|---|---|
| Advertising | thấp, tỉ lệ với tần suất adv | tăng adv interval = tiết kiệm, đổi lấy thời gian phát hiện lâu hơn |
| Kết nối (peripheral) | thấp–trung bình | connection interval và slave latency quyết định |
| Scan | cao — tương đương radio bật liên tục | tránh scan liên tục trên thiết bị pin |

BLE rẻ hơn Wi-Fi rất nhiều cho truyền dữ liệu nhỏ, thưa. Nếu bài toán là "gửi vài chục byte mỗi
vài phút tới một gateway gần", BLE hoặc ESP-NOW thường là lựa chọn đúng còn Wi-Fi là sai —
nhưng đổi lại cần gateway, tức là **thay đổi kiến trúc hệ thống**, không phải một tuỳ chọn cấu
hình. Nêu như một lựa chọn kiến trúc, để người dùng quyết.

Tăng advertising interval hoặc connection interval: tiết kiệm rõ, nhưng làm app di động kết nối
chậm hơn và cảm giác "thiết bị lag". Thuộc trục Latency, phải công bố.

## ESP-NOW

Không cần kết hợp với AP, không DHCP, không handshake — pha radio ngắn hơn hẳn Wi-Fi
(thường vài ms cho một gói). Rất hợp thiết bị pin gửi dữ liệu nhỏ tới một gateway.

Đánh đổi: không có TCP/TLS (bảo mật phải tự làm ở tầng ứng dụng), không có ack tầng cao ngoài ack
MAC, cần một thiết bị thu luôn bật. Chi tiết giao thức → `esp32-05-connectivity/references/ble-espnow.md`;
bảo mật payload → `esp32-10-security`.

## Kiểm chứng mọi thay đổi ở file này

Mỗi kỹ thuật áp dụng phải kèm đo lại **cả hai** mặt:
- năng lượng: mAs của pha radio, trước và sau;
- độ tin cậy: tỉ lệ kết nối thành công, tỉ lệ fallback quét, độ trễ nhận lệnh — đo ở điều kiện
  **sóng yếu**, không chỉ cạnh router.

Tiết kiệm 30% điện mà tỉ lệ gửi thành công tụt từ 99% xuống 90% là một đánh đổi tồi và phải được
trình bày như vậy, không phải báo cáo là "đã tối ưu".
