# Báo cáo RCA — <tiêu đề lỗi ngắn gọn>

Ngày: <YYYY-MM-DD> | Firmware: <version / git hash> | Chip: <module chính xác> | IDF: <version>

---

## 1. Symptom

| Mục | Nội dung |
|---|---|
| Biểu hiện | <crash / reset / treo / dữ liệu sai> |
| Xảy ra khi | <lúc boot / sau N phút / khi có sự kiện X> |
| Tần suất | <mỗi lần / 1 trong N / mỗi vài giờ> |
| Tái hiện | <có, bằng cách... / không tái hiện được trên bàn> |
| Lần cuối chạy đúng | <commit / ngày> |
| Đã đổi gì từ đó | <code / phần cứng / nguồn / môi trường> |

## 2. Evidence

```
[Hạng 1] <log, backtrace đã giải mã, core dump>
[Hạng 2] <số đo: heap, high-water mark, run-time stats, điện áp>
[Hạng 3] <kết quả tái hiện có kiểm soát>
[Hạng 4] <suy luận từ code — đánh dấu rõ là suy luận>
[THIẾU]  <bằng chứng chưa có + cách lấy>
```

ELF dùng để giải mã: `<đường dẫn>` — khớp với firmware đang chạy: <có / không>

## 3. Hypothesis

| ID | Giả thuyết | Phép thử bác bỏ |
|---|---|---|
| H1 | | |
| H2 | | |
| H3 | | |

## 4. Verification

| ID | Thí nghiệm đã chạy | Kết quả |
|---|---|---|
| H1 | | XÁC NHẬN / BÁC BỎ / CHƯA THỬ ĐƯỢC |
| H2 | | |

Mỗi thí nghiệm chỉ đổi một biến. Ghi rõ cấu hình debug đang bật khi đo.

## 5. Root Cause

**Nguyên nhân:** <một đoạn, cụ thể>

**Vị trí:** `<file>:<line>` hoặc `<chân / linh kiện>`

Giải thích đầy đủ:
- Vì sao lỗi xảy ra: <...>
- Vì sao tần suất đúng như quan sát: <...>
- Vì sao trước đây không lỗi: <...>
- Các triệu chứng phụ được giải thích: <...>

> Còn triệu chứng nào chưa giải thích được ⇒ chưa phải root cause, quay lại bước 3.

## 6. Minimal Fix

**Thay đổi:** <mô tả thay đổi nhỏ nhất>

**Vì sao nó triệt tiêu nguyên nhân:** <...>

**Tác dụng phụ đã xét:** RAM <...> | timing <...> | đường lỗi khác <...> | hành vi lúc boot <...>

**Không nằm trong danh sách cấm:** <xác nhận — không delay, không tăng stack vô căn cứ,
không tắt watchdog, không restart định kỳ, không tắt brownout>

`[VÁ TẠM]` (nếu có): <mô tả> — che: <cái gì> — phải gỡ khi: <điều kiện>

## 7. Regression Test

| Mục | Nội dung |
|---|---|
| Test phát hiện lại lỗi | <mô tả> |
| Chạy ở đâu | host / Unity trên target / HIL / soak |
| Ngưỡng assert | <ví dụ: high-water mark > 512 byte; free heap sau 24h không giảm quá 5%> |
| Giao cho | `esp32-08-testing` |

## 8. Còn treo

1. <giả thuyết chưa xác minh được, hoặc bằng chứng còn thiếu — kèm cách lấy>

## 9. Phòng ngừa (tuỳ chọn)

Lớp lỗi này còn có thể xuất hiện ở đâu nữa trong dự án? Cần đổi gì ở mức thiết kế?
→ `esp32-03-firmware-architecture`
