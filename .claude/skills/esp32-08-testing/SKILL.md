---
name: esp32-08-testing
description: Kiểm thử firmware ESP32 theo ba tầng — build/static validation (build đa target, warning, kích thước binary), software testing (host test, Unity trên target, pytest-embedded, integration, fault injection, regression), và hardware validation (peripheral, soak, HIL). Kèm Definition of Done bắt buộc cho feature production. Dùng khi dựng bộ test, chặn hồi quy sau khi sửa bug, hoặc chứng minh một tính năng đã thật sự xong.
---

# 08 — Testing

## Điều kiện vào
Chưa rõ chip / framework / phiên bản IDF → chạy Context Gate ở `esp32-firmware` trước.
Cổng an toàn và ngân sách context của `esp32-firmware` áp dụng nguyên vẹn tại đây.

Xây dựng **bằng chứng** rằng firmware đúng, và giữ nó đúng qua thời gian.

## Nguyên tắc tối thượng: compile ≠ xong

"Build thành công" chỉ chứng minh cú pháp hợp lệ. Nó không chứng minh code chạy,
không chứng minh chạy đúng, không chứng minh không làm hỏng thứ khác.

Khi báo cáo, **luôn nói rõ đã kiểm chứng tới mức nào**, dùng đúng các từ này:

| Mức | Nghĩa |
|---|---|
| `BUILD` | biên dịch được, chưa chạy dòng nào |
| `HOST` | logic thuần đã chạy và pass trên máy tính |
| `QEMU` | firmware boot và chạy trong mô phỏng, không có ngoại vi thật |
| `TARGET` | đã chạy trên chip thật, có log chứng minh |
| `HARDWARE` | đã chạy với ngoại vi/thiết bị thật, kết quả đo được xác nhận |
| `SOAK` | đã chạy liên tục <N> giờ/ngày, không suy giảm |

Không được viết "đã hoạt động", "đã fix", "đã xong" khi mức kiểm chứng mới là `BUILD`.
Không được suy diễn mức cao hơn mức thực sự đã chạy. Chưa có phần cứng để chạy →
nói rõ "mới tới `BUILD`, cần bạn chạy trên board để lên `TARGET`".

## Ba tầng kiểm thử

| Tầng | Tên | Chạy ở đâu | Bắt loại lỗi nào | Chi phí |
|---|---|---|---|---|
| 1 | Build / static validation | CI, máy dev | sai API, sai target, warning, tràn flash | rẻ nhất, tự động hoàn toàn |
| 2 | Software testing | host + board | logic sai, tích hợp sai, hồi quy | rẻ, phần lớn giá trị nằm ở đây |
| 3 | Hardware validation | board + thiết bị đo | timing, điện, nhiễu, độ bền | đắt nhất, không thể thay thế |

Tầng dưới không thay được tầng trên. Host test pass không có nghĩa I2C chạy được.
Tầng trên cũng không thay được tầng dưới: chạy thử trên board một lần không chặn được hồi quy.

**Đi từ tầng 1 lên.** Lỗi bắt được ở tầng 1 rẻ hơn tầng 3 hàng trăm lần.

### Tầng 1 — Build / static validation → `references/static-validation.md`
Build cho mọi chip target được hỗ trợ; warning coi như lỗi; static analysis; kiểm tra
kích thước binary so với slot OTA; kiểm tra sdkconfig và partition table.

### Tầng 2 — Software testing → `references/software-testing.md`
Host test cho logic thuần; Unity test trên target cho driver; integration test cho
đường dữ liệu; regression test cho mọi bug đã sửa; fault injection khi phù hợp
(`references/fault-injection.md`).

### Tầng 3 — Hardware validation → `references/hardware-validation.md`
Peripheral test (từng ngoại vi với thiết bị thật), communication test (mạng, rớt kết nối),
điện/timing đo bằng máy, soak test nhiều ngày, HIL trong CI nếu có.

## Điều kiện tiên quyết

Phần lớn giá trị nằm ở host test, và nó **chỉ khả thi khi logic đã tách khỏi I/O**.
Nếu business logic gọi thẳng `gpio_set_level()` hay `i2c_master_transmit()` thì việc
đầu tiên không phải viết test, mà là tách lớp → `esp32-03-firmware-architecture`.

Không dựng mock khổng lồ cho toàn bộ ESP-IDF để ép test driver trên host. Driver test
trên target. Mock chỉ đặt ở đúng một ranh giới đã thiết kế sẵn.

## Definition of Done

Mọi feature **production** phải qua checklist `checklists/definition-of-done.md`
trước khi được gọi là xong. Không đủ mục nào thì ghi rõ mục đó chưa đạt — không làm tròn.

Tóm tắt (chi tiết trong checklist):
1. Build sạch cho mọi target được hỗ trợ, không warning mới.
2. Logic thuần có host test, chạy trong CI.
3. Đã chạy trên phần cứng thật, có log/số đo chứng minh.
4. Đường lỗi đã được thử, không chỉ đường thành công.
5. Có regression test nếu feature này từng sinh bug.
6. Hành vi sau reset / mất nguồn / mất mạng đã xác nhận.
7. Không rò heap sau chu kỳ lặp lại.
8. Log và mã lỗi đủ để chẩn đoán từ xa.
9. Đã cập nhật tài liệu/cấu hình đi kèm.

Mức simple (xem `esp32-03-firmware-architecture/references/tiers.md`) không bắt buộc
đủ 9 mục — nhưng phải nói rõ bỏ mục nào và vì sao.

## Quy trình khi được giao việc test

1. Hỏi/xác định **mức dự án** và phạm vi: dựng bộ test mới, thêm test cho một feature,
   hay chặn một bug vừa sửa?
2. Phân loại cái cần test vào 3 tầng. Cái gì test được ở tầng thấp thì không đẩy lên cao.
3. Viết test. Bug đã sửa → **test chặn hồi quy trước, nó phải fail trên code cũ**.
4. Chạy được cái gì thì chạy, báo cáo đúng mức kiểm chứng.
5. Cái chưa chạy được (thiếu phần cứng, thiếu thiết bị đo) → liệt kê thành việc cần
   user làm, kèm cách chạy và kết quả mong đợi.

## References
- `checklists/definition-of-done.md` — DoD 9 mục cho feature production
- `templates/test-plan.md` — mẫu kế hoạch test theo 3 tầng
- `references/static-validation.md` — tầng 1
- `references/software-testing.md` — tầng 2: host, unit, integration, regression
- `references/fault-injection.md` — khi nào và cách tiêm lỗi
- `references/hardware-validation.md` — tầng 3: peripheral, communication, soak, HIL
- `references/unity-and-ci.md` — cú pháp Unity trên target, cấu hình CI đa target

## Không thuộc scope
- Điều tra một crash cụ thể đang xảy ra → `esp32-07-debugging`
- Đo stack/heap để tối ưu → `esp32-09-performance-optimization`
- Checklist rà soát và quy trình release → `esp32-12-release`
- Tách lớp để code test được → `esp32-03-firmware-architecture`

## Đầu ra
Kế hoạch test theo 3 tầng + test đã viết + báo cáo ghi rõ mức kiểm chứng đạt được
và danh sách việc cần chạy trên phần cứng.
