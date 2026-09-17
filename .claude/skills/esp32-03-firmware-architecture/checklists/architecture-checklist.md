# Checklist kiến trúc — 14 hạng mục

Mỗi hạng mục kết luận: **CẦN** / **HOÃN** (ghi điều kiện kích hoạt) / **KHÔNG CẦN**.
"Không chắc" không được làm tròn thành CẦN. Không chắc thì hỏi.

## 1. Application layer
- Nghiệp vụ sản phẩm đã tách khỏi I/O chưa?
- Có file nào vừa quyết định nghiệp vụ vừa đụng thanh ghi không?
- Mức simple: `app_main` là đủ. Đừng tạo lớp app cho một vòng lặp.

## 2. Service layer
- Có logic nào được ≥2 nơi dùng không?
- Có backend nào có thể đổi (MQTT ↔ HTTP, cảm biến A ↔ B) không?
- Nếu service chỉ chuyển tiếp lời gọi xuống driver → bỏ, gọi thẳng.

## 3. Driver layer
- Mỗi ngoại vi / chip ngoài có đúng một chủ sở hữu chưa?
- Có hai nơi cùng cấu hình một ngoại vi không? (nguồn lỗi chập chờn kinh điển)
- Driver có trả giá trị vật lý (°C, mA) thay vì raw không?

## 4. Hardware abstraction
- Có kế hoạch đổi chip/board không? Có cần chạy logic trên host không?
- Không có cả hai → KHÔNG CẦN. HAL suông chỉ thêm một lớp gián tiếp.

## 5. Configuration
- Mỗi giá trị đã xếp đúng: board / build-time / runtime per-device / tạm?
- Có bí mật nào đang nằm trong source không?
- NVS rỗng (thiết bị mới) có boot được không?

## 6. Communication
- Ranh giới nào là in-process (queue/hàm), ranh giới nào ra khỏi thiết bị (mạng)?
- Mỗi cặp task đã chọn đúng cơ chế và ghi lý do chưa?
- Hành vi khi queue đầy đã quyết định chưa?

## 7. Event system
- Có sự kiện nào ≥2 bên quan tâm không? Không có → KHÔNG CẦN.
- Nếu có: thứ tự handler có quan trọng không? Quan trọng → thiết kế đang sai.

## 8. Task architecture
- Mỗi task có một nhịp/nguồn block riêng không?
- Có hai task luôn chạy nối tiếp nhau không? (gộp)
- Priority, core, stack đã điền đủ bảng chưa? Có task nào ≥18 không? (cấm)
- Task nào có thể block lâu? Nó có làm kẹt cái gì không?

## 9. State machine
- Hành vi có phụ thuộc lịch sử không? Có timeout/retry không?
- Mọi state có đường ra chưa? Mọi state chờ có timeout chưa?
- Hàm chuyển trạng thái có thuần (không I/O) không?

## 10. Error handling
- Đã phân loại fatal / retry / degrade chưa?
- `ESP_ERROR_CHECK` chỉ còn ở init, không ở đường runtime?
- Lỗi có ngữ cảnh khi đi lên không, hay nuốt mất?
- Cơ cấu chấp hành có trạng thái an toàn khi lỗi không? (relay, van, motor)

## 11. Logging
- Ai đọc log ngoài thực địa? Bằng cách nào?
- Có log trùng ở nhiều lớp cho cùng một lỗi không?
- Có log nào trong ISR hoặc vòng lặp nóng không?
- Production: đổi được log level lúc chạy chưa? Lỗi nghiêm trọng có lưu lại không?

## 12. Storage
- Dữ liệu nào phải sống qua reset / qua OTA / qua mất điện đột ngột?
- NVS schema có version chưa?
- Mọi lần đọc có xử lý key thiếu / dữ liệu hỏng chưa?
- Có ghi flash trong vòng lặp không?

## 13. OTA
- Có OTA không? (quyết định NGAY, vì ràng buộc partition)
- Partition đã chừa `ota_0`/`ota_1`/`otadata` chưa? Binary có vừa nửa flash không?
- Có rollback tự động + self-test sau cập nhật chưa?
- Dữ liệu người dùng có nằm ngoài app partition không?

## Rà soát cuối

- Có phụ thuộc vòng nào không? (`grep` include ngược hướng)
- Có global chia sẻ giữa component không?
- Mỗi header công khai có đủ 5 dòng hợp đồng (context / blocking / thread / bộ nhớ / lỗi)?
- Số file có nhiều hơn số chức năng thật không?
- Có lớp nào không nói được nó chặn cái đau cụ thể nào không? → bỏ.

## 14. Fault tolerance
- [ ] TWDT đã bật chưa? Task nào subscribe, task nào cố tình không (và vì sao)?
- [ ] Mỗi task subscribe feed đúng **một** chỗ ở đầu vòng lặp, không rải giữa vòng lặp?
- [ ] Timeout watchdog có cơ sở (≈3× chu kỳ chậm nhất hợp lệ), đã ghi lý do chưa?
- [ ] Cơ cấu chấp hành về trạng thái an toàn bằng **phần cứng** khi reset/panic?
- [ ] Có bộ đếm boot thất bại + safe mode **OTA được** chưa? (production)
- [ ] Có factory reset không cần cáp, và tách namespace NVS cấu hình / danh tính nhà máy chưa?
- [ ] Lưới an toàn đã được thử thật (ép treo task, ép boot loop 3 lần) chưa?

Chi tiết: `references/fault-tolerance.md`.
