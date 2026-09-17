# Definition of Done — feature production

Một feature chỉ được gọi là **xong** khi đủ 9 mục dưới. Mỗi mục: **ĐẠT** / **CHƯA ĐẠT** /
**KHÔNG ÁP DỤNG** (kèm lý do). Không có mục "gần đạt". Không làm tròn lên.

Mức simple/medium không bắt buộc đủ 9 — nhưng phải liệt kê mục bỏ qua và lý do, không im lặng.

---

## 1. Build sạch
- [ ] Build thành công cho **mọi chip target** dự án hỗ trợ.
- [ ] Không warning mới so với trước khi thêm feature.
- [ ] Kích thước app vẫn nằm trong slot OTA, còn dư biên.
- [ ] Static analysis không phát sinh phát hiện mức cao mới.

## 2. Logic thuần có test tự động
- [ ] Phần tính toán/quyết định của feature nằm trong file không phụ thuộc ESP-IDF.
- [ ] Có host test phủ: giá trị bình thường, giá trị biên, đầu vào hỏng.
- [ ] Test chạy trong CI, chặn merge khi đỏ.
- [ ] Nếu không có logic thuần nào → ghi KHÔNG ÁP DỤNG kèm lý do.

## 3. Đã chạy trên phần cứng thật
- [ ] Đã nạp và chạy trên board thật, không chỉ QEMU.
- [ ] Có **bằng chứng**: đoạn log, số đo, hoặc ảnh scope — không phải "tôi thấy nó chạy".
- [ ] Ngoại vi mới dùng đã được kiểm chứng riêng (`references/hardware-validation.md`).

## 4. Đường lỗi đã được thử
- [ ] Không chỉ thử đường thành công.
- [ ] Đã thử ít nhất: thiết bị/đối tác không phản hồi, dữ liệu sai, timeout.
- [ ] Mỗi lỗi có hành vi đã định trước: retry / degrade / fail-safe. Không treo, không crash.
- [ ] Cơ cấu chấp hành (relay, van, motor) về **trạng thái an toàn** khi lỗi, và cả lúc boot.

## 5. Regression test
- [ ] Bug nào từng xuất hiện ở feature này đều có test tái hiện, đã từng fail trên code cũ.
- [ ] Test ghi rõ triệu chứng + nguyên nhân gốc + ngày/issue trong comment.
- [ ] Feature không phá test nào đang có.

## 6. Hành vi qua reset / mất nguồn / mất mạng
- [ ] Reset giữa chừng: thiết bị boot lại vào trạng thái hợp lệ.
- [ ] Cắt nguồn khi đang ghi storage: dữ liệu cũ hoặc mới, không phải rác.
- [ ] Mất mạng: dữ liệu được đệm hoặc bỏ **theo đúng thiết kế đã chốt**, không mất im lặng.
- [ ] Dữ liệu feature này ghi có sống qua OTA không? (nếu dự án có OTA)
- [ ] NVS rỗng / cấu hình thiếu: vẫn boot được với mặc định an toàn.

## 7. Tài nguyên ổn định
- [ ] Chạy lặp chu kỳ của feature ≥ vài trăm lần: free heap không trôi xuống.
- [ ] Stack high-water mark của task liên quan còn dư biên (≥ 25%).
- [ ] Không thêm cấp phát/giải phóng lặp lại trong vòng lặp nóng.
- [ ] Không làm chậm đường thời gian thực đang có (đo, đừng đoán).

## 8. Quan sát được từ xa
- [ ] Có log ở mốc quyết định và ở mọi lỗi, đúng mức E/W/I.
- [ ] Log đủ để chẩn đoán **mà không cần cắm cáp**: có ngữ cảnh, có mã lỗi, không log trùng ở nhiều lớp.
- [ ] Không log trong ISR, không log tần suất cao trong vòng lặp nóng.
- [ ] Lỗi nghiêm trọng lưu lại được (NVS/core dump) nếu là production.

## 9. Đi kèm đầy đủ
- [ ] Cấu hình mới có mặc định an toàn, có trong Kconfig/`board_config.h`, không hard-code bí mật.
- [ ] Tài liệu kiến trúc/pin map/test plan đã cập nhật nếu feature làm chúng lệch.
- [ ] Hợp đồng interface (context / blocking / thread / bộ nhớ / lỗi) đã ghi trong header công khai.
- [ ] Đã ghi mức kiểm chứng đạt được: `BUILD` / `HOST` / `QEMU` / `TARGET` / `HARDWARE` / `SOAK`.

---

## Mẫu báo cáo DoD

```
Feature: <tên>            Mức kiểm chứng: TARGET
| # | Mục | Kết quả | Ghi chú |
|---|---|---|---|
| 1 | Build sạch | ĐẠT | esp32 + s3 + c6, app 68% slot OTA |
| 2 | Host test | ĐẠT | 14 case, phủ biên và CRC sai |
| 3 | Phần cứng thật | ĐẠT | log kèm dưới, đo 3 board |
| 4 | Đường lỗi | CHƯA ĐẠT | chưa thử cảm biến rút giữa chừng |
| ... |
Chưa đạt: #4 — cần rút dây SDA khi đang đo, xác nhận timeout ≤ 500 ms và không treo.
```

Có bất kỳ mục CHƯA ĐẠT nào → feature **chưa xong**. Nói thẳng, kèm việc còn thiếu và
cách chạy nó. Không gọi là "xong nhưng còn vài điểm nhỏ".
