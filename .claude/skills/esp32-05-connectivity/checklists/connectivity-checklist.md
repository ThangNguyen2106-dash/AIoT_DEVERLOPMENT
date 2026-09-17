# Checklist rà soát code kết nối

Chạy trước khi coi một đường mạng là "xong". Mỗi mục chưa đạt là một lỗi sẽ xuất hiện ngoài
hiện trường, không phải trên bàn.

## Khởi tạo
- [ ] Mọi hàm init đều kiểm tra mã trả về; lỗi init được phân loại **INIT_FAIL** và vào FAULT, không retry.
- [ ] `nvs_flash_init()` có xử lý `NO_FREE_PAGES` / `NEW_VERSION_FOUND`.
- [ ] Đã xác nhận phần cứng có ngoại vi đang dùng (EMAC, BLE, PSRAM cho buffer lớn).
- [ ] `ESP_ERROR_CHECK` chỉ dùng cho lỗi thật sự không thể tiếp tục, không dùng cho lỗi runtime.

## State machine
- [ ] Có enum state rõ ràng; ứng dụng chỉ hỏi `net_is_ready()`, không tự đoán.
- [ ] Vào `READY` chỉ khi đủ điều kiện: link + IP + session + giờ (nếu có TLS).
- [ ] Rời `READY` được thông báo ngay cho tầng ứng dụng.
- [ ] Có deadline tổng cho `CONNECTING`; quá hạn thì huỷ sạch và về BACKOFF.
- [ ] `FAULT` không tự thoát bằng retry.

## Timeout
- [ ] Không có `portMAX_DELAY` trên bất kỳ thao tác mạng nào.
- [ ] Không có `while (!connected) delay()` trong code ứng dụng.
- [ ] Mọi socket có `SO_RCVTIMEO` / `SO_SNDTIMEO`; mọi HTTP có `timeout_ms`.
- [ ] Mỗi giá trị timeout có lý do viết ra được, không phải số bịa.
- [ ] Timeout cho cellular được nới đúng mức (PPP 60 s, TLS 30 s), không dùng số của Wi-Fi.

## Retry và backoff
- [ ] Backoff có **jitter** và có **trần**.
- [ ] `attempt` chỉ reset khi kết nối đã ổn định (≥ 30 s ở READY), không reset ngay khi vừa connect.
- [ ] AUTH_FAIL không retry nhanh (≥ 5 phút) và có báo trạng thái ra ngoài.
- [ ] PROTO_FAIL không retry cùng payload.
- [ ] SERVER_FAIL backoff dài hơn LINK_FAIL và tôn trọng `Retry-After`.
- [ ] Có đổi chiến lược sau N lần hỏng (quét lại, đổi link, hạ tần suất), không lặp mãi một cách.
- [ ] Không gọi `connect()` ngay trong event handler `DISCONNECTED`.

## Phân loại lỗi
- [ ] Có enum 5 lớp lỗi và hàm ánh xạ từ mã gốc (wifi reason, errno, CONNACK, HTTP status, mbedTLS).
- [ ] Mã lỗi gốc được giữ lại và log, không chỉ giữ lớp.
- [ ] HTTP: kiểm tra status code, không chỉ `err == ESP_OK`.
- [ ] MQTT: đọc `connect_return_code` để tách AUTH / PROTO / SERVER.
- [ ] WebSocket: đọc status của handshake, tách 401 với 400 với 5xx.

## Offline và buffering
- [ ] Offline là trạng thái hợp lệ; chức năng an toàn và điều khiển cục bộ vẫn chạy.
- [ ] Lệnh từ cloud có hạn hiệu lực; mất mạng quá lâu thì cơ cấu chấp hành về trạng thái an toàn.
- [ ] Hàng đợi có **giới hạn cứng**, không `malloc` không trần.
- [ ] Chính sách khi đầy được chọn có chủ đích (telemetry bỏ cũ nhất; alarm có hàng đợi riêng).
- [ ] Message chỉ bị xoá khỏi buffer sau khi có xác nhận đã gửi (PUBACK / HTTP 2xx).
- [ ] Xả buffer sau khi nối lại có tiết chế, không xả một phát toàn bộ.
- [ ] Message có id/timestamp để server khử trùng lặp.
- [ ] Bản ghi sinh ra khi chưa có giờ được đánh dấu rõ, không gửi mốc 1970 như thời gian thật.

## TLS và xác thực
- [ ] Không có bất kỳ chỗ nào tắt verify chứng chỉ.
- [ ] SNTP đồng bộ trước handshake; "đã có giờ" là điều kiện vào READY.
- [ ] Có kế hoạch cập nhật CA trước khi CA hết hạn.
- [ ] Bí mật không nằm trong source (→ `esp32-10-security`).
- [ ] Task chạy TLS có stack ≥ 8 KB; đã đo heap với số kết nối TLS đồng thời tối đa.

## Tài nguyên
- [ ] Mọi đường thoát khỏi hàm đều `cleanup()` / `close()` — kể cả nhánh lỗi.
- [ ] Client MQTT/HTTP không bị tạo lại mà không destroy cái cũ.
- [ ] Đã chạy thử dài (≥ vài giờ) và kiểm tra heap free không giảm dần.
- [ ] Số socket đồng thời nằm trong `CONFIG_LWIP_MAX_SOCKETS`.

## Callback
- [ ] Callback của stack mạng không blocking, không làm việc nặng, không chứa nghiệp vụ.
- [ ] Dữ liệu trong callback được copy trước khi đẩy queue (con trỏ hết hạn khi callback trả về).

## Quan sát
- [ ] Có counter: attempts, success, disconnects, last_err (lớp + mã), uptime online, msg queued/sent/dropped.
- [ ] Có RSSI/CSQ; cellular có thêm byte đã dùng.
- [ ] Log mỗi lần chuyển state kèm lý do; đủ để chẩn đoán từ xa mà không cần cắm dây.

## Kiểm thử bắt buộc (không chỉ test happy path)
- [ ] Rút mạng giữa lúc đang gửi → thiết bị phục hồi, dữ liệu không mất.
- [ ] Sai mật khẩu → không retry bão, có báo trạng thái.
- [ ] Broker tắt rồi bật lại → tự nối lại, buffer được xả đúng.
- [ ] Server trả 500 liên tục → backoff dài, buffer giữ nguyên.
- [ ] Đặt giờ sai rồi bật TLS → lỗi được chẩn đoán đúng là vấn đề thời gian.
- [ ] Chạy offline qua đêm → không hết heap, không reset, dữ liệu xử lý đúng chính sách.
- [ ] Reboot giữa lúc mất mạng → hành vi đúng như thiết kế đã nêu về tính bền của buffer.
