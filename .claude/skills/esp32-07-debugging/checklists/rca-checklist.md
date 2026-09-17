# Cổng kiểm tra RCA — chạy trước khi kết luận

Mỗi mục **PASS / FAIL**. Còn một FAIL thì **chưa được trình bày kết luận**, chỉ được trình bày
tiến độ điều tra và danh sách bằng chứng còn thiếu.

## Cổng 1 — Symptom đã cụ thể
- [ ] Biết lỗi xảy ra **khi nào** (lúc boot / sau N phút / khi có sự kiện X).
- [ ] Biết **tần suất** (mỗi lần / 1 trong N lần / mỗi vài giờ).
- [ ] Biết **tái hiện được không** và tái hiện bằng cách nào.
- [ ] Biết **lần cuối chạy đúng là khi nào** và đã đổi gì từ đó.
- [ ] Phân biệt được: crash (có panic) / reset (không panic) / treo (không reset) — ba thứ khác nhau.

## Cổng 2 — Evidence đủ hạng
- [ ] Có ít nhất một bằng chứng **hạng 1 hoặc 2** (xem thang bằng chứng ở SKILL.md).
- [ ] Log bắt đầu từ dòng `rst:0x...`, không bị cắt đầu.
- [ ] Backtrace (nếu có) giải mã bằng **đúng ELF của firmware đang chạy**.
- [ ] Mọi bằng chứng hạng 4–5 đã được đánh dấu rõ là suy đoán.

## Cổng 3 — Hypothesis bác bỏ được
- [ ] Mỗi giả thuyết nêu rõ **phép thử nào sẽ bác bỏ nó**.
- [ ] Không có giả thuyết kiểu "do nhiễu", "do chip lỗi", "do thư viện dở" khi chưa có bằng chứng.
- [ ] Có ít nhất một giả thuyết thay thế (không chỉ bám một hướng duy nhất).

## Cổng 4 — Verification thật sự đã chạy
- [ ] Mỗi giả thuyết có kết quả: XÁC NHẬN / BÁC BỎ / CHƯA THỬ ĐƯỢC.
- [ ] Không dùng "sửa thử thấy hết lỗi" làm bằng chứng xác nhận (lỗi hiếm sẽ đánh lừa).
- [ ] Mỗi lần thử chỉ đổi **một** biến.
- [ ] Thí nghiệm tái hiện được: chạy lại cho kết quả như cũ.

## Cổng 5 — Root Cause giải thích hết
- [ ] Giải thích được **vì sao lỗi xảy ra**.
- [ ] Giải thích được **vì sao tần suất đúng như quan sát** (vì sao hiếm / vì sao chỉ khi có tải).
- [ ] Giải thích được **vì sao trước đây không lỗi** (nếu là hồi quy).
- [ ] Chỉ ra được **vị trí cụ thể**: file:line, hoặc chân/linh kiện cụ thể.
- [ ] Không còn triệu chứng nào trong báo cáo bị bỏ lại chưa giải thích.

## Cổng 6 — Minimal Fix đúng nghĩa tối thiểu
- [ ] Bản vá tác động trực tiếp lên nguyên nhân, không lên triệu chứng.
- [ ] Không nằm trong danh sách cấm ở SKILL.md (delay, tăng stack, tắt watchdog, restart...).
- [ ] Không kèm refactor, đổi tên, dọn code — những thứ đó tách ra việc khác.
- [ ] Nêu rõ **vì sao bản vá này triệt tiêu nguyên nhân**, không chỉ "sửa như vậy là được".
- [ ] Đã xét tác dụng phụ: RAM, timing, đường lỗi khác, hành vi lúc boot và lúc mất nguồn.
- [ ] Nếu có `[VÁ TẠM]`: đã gắn nhãn và nói rõ nó che cái gì.

## Cổng 7 — Regression Test tồn tại
- [ ] Có mô tả test/kịch bản **sẽ fail nếu bug quay lại**.
- [ ] Nêu rõ test chạy ở đâu: host / target Unity / HIL / soak.
- [ ] Với bug chỉ xuất hiện theo thời gian (rò heap): có ngưỡng đo được, ví dụ
      "free heap sau 24h không giảm quá 5% so với sau 1h".
- [ ] Việc hiện thực test được chuyển cho `esp32-08-testing`, không viết ở đây.

## Cổng an toàn (bất biến toàn package)
- [ ] Không đề xuất tắt brownout detector.
- [ ] Không đề xuất tắt watchdog như một bản sửa.
- [ ] Bản vá không làm cơ cấu chấp hành mất trạng thái an toàn lúc boot / lúc panic.
- [ ] Không tự chạy lệnh flash/erase/burn — qua `esp32-firmware/checklists/pre-flash.md`.
