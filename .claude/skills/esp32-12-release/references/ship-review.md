# ESP32 — Rà soát trước khi ship

Rà soát theo 8 nhóm dưới đây. Với mỗi phát hiện, nêu **file:line**, hậu quả cụ thể ngoài thực địa,
và cách sửa. Xếp theo mức độ: chặn release / nên sửa / cải thiện.

Chỉ đọc phần code liên quan tới từng nhóm, không nạp cả repo.

## Quan hệ với Definition of Done

| | Definition of Done (`esp32-08-testing`) | Ship review (file này) |
|---|---|---|
| Phạm vi | **một feature** | **cả bản release**, toàn repo |
| Chạy khi | feature được coi là xong | chuẩn bị phát hành |
| Giả định | — | mọi feature đã qua DoD |

Checklist này **không lặp lại** DoD. Nếu một feature chưa có báo cáo DoD, đó tự nó là mục
**chặn release** — ghi vào kết luận, đừng kiểm lại thủ công từng mục DoD ở đây.
Cái file này tìm là vấn đề **cấp hệ thống**: thứ đúng ở từng feature nhưng sai khi ghép lại,
và thứ chỉ lộ ra khi chạy hàng tháng ngoài thực địa.

## 1. An toàn phần cứng (chặn release nếu thiếu)
Quy tắc gốc: `esp32-04-driver-development/references/actuators.md`.
- Cơ cấu chấp hành có trạng thái an toàn lúc boot và lúc panic không?
- Có fail-safe khi mất liên lạc / mất cảm biến không (motor dừng, heater tắt)?
- Có giới hạn giá trị điều khiển để lệnh sai không phá hỏng thiết bị không?
- Chân dùng có phạm strapping pin hoặc chân flash không?

## 2. Xử lý lỗi
- Còn `esp_err_t` nào bị bỏ qua không?
- `ESP_ERROR_CHECK` có bị dùng cho lỗi runtime phục hồi được không (gây reset không cần thiết)?
- Có nhánh lỗi nào rò tài nguyên (không free, không đóng handle, không cleanup client) không?
- Có `while(1){}` câm hoặc nhánh lỗi không log gì không?

## 3. Bộ nhớ
- Còn malloc/free lặp lại trong vòng lặp nóng không (phân mảnh)?
- Buffer lớn khai báo cục bộ trên stack?
- Stack size có được xác định bằng đo high-water mark, hay đặt bừa?
- Có theo dõi `esp_get_minimum_free_heap_size()` để phát hiện rò dài hạn không?

## 4. Đồng thời
- Biến chia sẻ giữa task/ISR có được bảo vệ đúng không?
- ISR có ngắn, có `IRAM_ATTR`, có dùng đúng biến thể `FromISR` không?
- Có task nào spin không block (gây task watchdog) không?
- Thứ tự lấy khoá có nhất quán ở mọi nơi không?
- Có `portMAX_DELAY` ở chỗ mà treo vĩnh viễn là không chấp nhận được không?

## 5. Độ bền dài hạn
- Bộ đếm có tràn sau nhiều ngày chạy không (`uint32_t` ms tràn sau 49 ngày)?
- Ghi NVS/flash có quá thường xuyên gây mòn flash không?
- Mọi vòng lặp retry có backoff và có điều kiện dừng không?
- Đã chạy soak test liên tục nhiều ngày chưa?

## 6. Bảo mật
Chạy checklist ở `esp32-10-security/references/hardening.md`.

## 7. Cập nhật và chẩn đoán từ xa
- Có OTA với rollback và self-test thực chất không?
- Có core dump vào flash để chẩn đoán crash ngoài thực địa không?
- Có ghi phiên bản firmware, lý do reset gần nhất, số lần crash vào NVS không?
- Mức log production có hợp lý không (không quá nhiều, không câm hoàn toàn)?

## 8. Khả năng bảo trì
- Pin và tham số board có gom về một nơi không?
- Logic có tách khỏi driver để test trên host được không?
- Có "magic number" không giải thích trong code không?
- Có test tự động và CI build cho mọi chip target không?

## Kết luận

Kết thúc bằng một đánh giá thẳng thắn: sẵn sàng ship, hay còn bao nhiêu mục chặn release.
Nếu phát hiện rủi ro có thể gây hỏng phần cứng hoặc mất an toàn cho người dùng, nêu mục đó
lên đầu tiên.
