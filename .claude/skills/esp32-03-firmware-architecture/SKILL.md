---
name: esp32-03-firmware-architecture
description: Thiết kế kiến trúc firmware ESP32 — phân lớp app/service/driver/HAL, cấu hình, event, kiến trúc task FreeRTOS (priority, core, stack, queue/mutex), state machine, xử lý lỗi, logging, storage, OTA, watchdog và crash recovery. Phân mức simple/medium/production. Dùng khi dựng khung dự án mới, chốt ranh giới module và cơ chế đồng bộ, hoặc đánh giá kiến trúc đang có.
---

# 03 — Firmware Architecture

## Điều kiện vào
Chưa rõ chip / framework / phiên bản IDF → chạy Context Gate ở `esp32-firmware` trước.
Cổng an toàn và ngân sách context của `esp32-firmware` áp dụng nguyên vẹn tại đây.

Quyết định **hình dạng của firmware**: có những lớp nào, ai gọi ai, dữ liệu chảy ra sao,
bao nhiêu task, chúng đồng bộ thế nào. Đầu ra là bản thiết kế + khung file, không phải tính năng.

## Nguyên tắc tối thượng: kiến trúc vừa đủ

Kiến trúc thừa gây hại ngang kiến trúc thiếu. Mỗi lớp trừu tượng phải trả giá bằng
file, chỉ báo gián tiếp, RAM và thời gian đọc hiểu.

- **Không áp dụng kiến trúc production cho project nhỏ.** Blink LED + đọc cảm biến
  không cần service layer, event bus, hay dependency injection.
- **Không refactor kiến trúc hiện có nếu user chưa yêu cầu.** Được phép chỉ ra vấn đề
  và đề xuất; việc đổi là quyết định của user. Khi thêm tính năng vào code có sẵn,
  bám theo cấu trúc đang có kể cả khi nó không phải cách bạn sẽ chọn.
  - *Ngoại lệ*: khi user yêu cầu test (`esp32-08-testing`) mà logic đang dính chặt I/O,
    tách lớp là **điều kiện kỹ thuật** của việc được yêu cầu, không phải refactor tự phát.
    Được đề xuất phạm vi tách **tối thiểu** (chỉ phần cần test), vẫn phải hỏi trước khi sửa,
    và vẫn phải nêu phương án thay thế: giữ nguyên cấu trúc, test ở mức `TARGET`.
- Mỗi lần đề xuất thêm một lớp/cơ chế, phải nói được **nó chặn cái đau cụ thể nào**.
  Không nói được → không thêm.

## Quy trình

1. **Phân mức dự án** → `references/tiers.md`. Chốt simple / medium / production **trước**
   khi bàn bất kỳ chi tiết nào. Nếu thiếu dữ kiện (có OTA không? chạy pin? bao nhiêu thiết bị
   ngoài thực địa? tuổi thọ sản phẩm?) thì hỏi, đừng đoán cao lên.
2. **Phân lớp** → `references/layering.md`. Chốt danh sách component và hướng phụ thuộc.
3. **Thiết kế runtime** → `references/runtime-patterns.md`. Task, event, state machine.
4. **Chốt quy ước xuyên suốt** → `references/cross-cutting.md` (config, error, log, storage, OTA),
   `references/fault-tolerance.md` (watchdog, crash recovery, safe mode)
   và `references/coding-rules.md` (quy tắc code mà mọi skill sinh code khác phải theo).
5. **Báo cáo** theo mẫu dưới, rồi mới tạo khung file.

Chỉ đọc reference của bước đang làm.

## 14 hạng mục phải phân tích

Mỗi hạng mục cho một trong ba kết luận: **CẦN** (thiết kế ngay) /
**HOÃN** (mức hiện tại chưa cần, ghi lại điều kiện kích hoạt) / **KHÔNG CẦN**.

| # | Hạng mục | Câu hỏi quyết định | Reference |
|---|---|---|---|
| 1 | Application layer | Nghiệp vụ có tách được khỏi I/O không? | `layering.md` |
| 2 | Service layer | Có logic dùng lại bởi ≥2 nơi, hoặc cần đổi backend? | `layering.md` |
| 3 | Driver layer | Mỗi ngoại vi có một chủ sở hữu rõ ràng chưa? | `layering.md` |
| 4 | Hardware abstraction | Có cần đổi chip/board, hoặc test trên host không? | `layering.md` |
| 5 | Configuration | Giá trị nào build-time, runtime, per-device? | `cross-cutting.md` |
| 6 | Communication | Ranh giới nào là in-process, ranh giới nào ra ngoài thiết bị? | `runtime-patterns.md` |
| 7 | Event system | Một sự kiện có nhiều người quan tâm không? | `runtime-patterns.md` |
| 8 | Task architecture | Có bao nhiêu luồng hoạt động thật sự độc lập? | `runtime-patterns.md` |
| 9 | State machine | Hành vi có phụ thuộc lịch sử, có timeout/retry không? | `runtime-patterns.md` |
| 10 | Error handling | Lỗi nào fatal, lỗi nào retry, lỗi nào degrade? | `coding-rules.md` |
| 11 | Logging | Ai đọc log, đọc ở đâu, khi thiết bị đã ra thực địa? | `cross-cutting.md` |
| 12 | Storage | Dữ liệu nào phải sống qua reset / qua OTA / qua mất điện? | `cross-cutting.md` |
| 13 | OTA | Có cập nhật từ xa không? Nếu có, partition và rollback đã tính chưa? | `cross-cutting.md` |
| 14 | Fault tolerance | Watchdog bật chưa, ai feed? Boot loop thì sao? Có safe mode / factory reset không? | `fault-tolerance.md` |

## Ưu tiên thiết kế (dùng để phân xử khi có đánh đổi)

Xếp theo thứ tự, cái trên thắng cái dưới:

1. **Deterministic behavior** — cùng input cho cùng hành vi; timing có giới hạn trên
   biết trước; không phụ thuộc thứ tự chạy ngẫu nhiên giữa task.
2. **Clear interfaces** — mỗi component một header công khai, nói rõ: ai gọi, gọi từ
   context nào (task/ISR), blocking hay không, sở hữu bộ nhớ nào, trả lỗi gì.
3. **Loose coupling** — phụ thuộc một chiều, đi xuống. Không vòng. Không include ngược.
4. **Testability** — logic thuần tách khỏi I/O đủ để chạy được trên host không cần chip.
5. **Minimal dependencies** — ít component, ít thư viện ngoài, ít lớp. Khi hai thiết kế
   ngang nhau ở 4 tiêu chí trên, chọn cái ít thứ hơn.

Xung đột thường gặp: trừu tượng hoá thêm (tốt cho 4) làm giảm 1 và 5 → chỉ làm khi
có nhu cầu thật (đổi chip, hai biến thể sản phẩm, logic đáng test).

## Mẫu báo cáo

```
## Mức dự án
<simple / medium / production> — căn cứ: <OTA? số thiết bị? tuổi thọ? số người maintain?>

## Phân lớp
app/      <tên> — <trách nhiệm một dòng>
service/  <tên> — ...
driver/   <tên> — ...
hal/      <có/không, lý do>
Hướng phụ thuộc: app → service → driver → hal. Cấm ngược.

## Sơ đồ task
| Task | Prio | Stack | Core | Block trên | Chu kỳ |
|---|---|---|---|---|---|
| sensor_task | 5 | 3072 | 1 | timer 1s | 1 Hz |
| net_task    | 4 | 4096 | 0 | queue tx  | sự kiện |

## Giao tiếp
| Từ → Đến | Cơ chế | Lý do |
|---|---|---|
| ISR → sensor_task | task notification | 1-1, không mất dữ liệu, rẻ nhất |
| sensor_task → net_task | queue (depth 8) | tách nhịp, chịu được net chậm |

## 14 hạng mục
1 Application layer   CẦN   — ...
4 Hardware abstraction HOÃN — kích hoạt khi: port sang S3 hoặc cần test host
7 Event system        KHÔNG CẦN — chỉ 2 task, queue trực tiếp là đủ
...

## Rủi ro kiến trúc
<mỗi mục: vấn đề — hệ quả — cách chặn>

## Câu hỏi còn treo
<những chỗ phải hỏi user thay vì đoán>
```

## References
- `checklists/architecture-checklist.md` — 14 hạng mục, câu hỏi quyết định từng mục, rà soát cuối
- `templates/architecture-doc.md` — mẫu tài liệu kiến trúc để điền
- `references/tiers.md` — phân mức simple / medium / production, cái gì thuộc mức nào
- `references/layering.md` — 4 lớp, hợp đồng interface, quy tắc phụ thuộc
- `references/runtime-patterns.md` — task, IPC, event system, state machine
- `references/cross-cutting.md` — config, logging, storage, OTA
- `references/coding-rules.md` — quy ước lỗi, log, ISR, bộ nhớ, thời gian
- `references/fault-tolerance.md` — watchdog, panic, bộ đếm crash, safe mode, factory reset

## Không thuộc scope
- Chọn chip, pin map, xung đột ngoại vi → `esp32-02-hardware-analysis`
- Cây thư mục, CMakeLists, partition table cụ thể → `esp32-01-project-init`
- Code driver ngoại vi → `esp32-04-driver-development`
- State machine nghiệp vụ cụ thể, schema dữ liệu → `esp32-06-application-development`
- Reconnect/backoff mạng → `esp32-05-connectivity`
- Gỡ race condition / deadlock đã xảy ra → `esp32-07-debugging`
- Đo stack high-water mark, tối ưu RAM → `esp32-09-performance-optimization`
- Triển khai esp_https_ota, ký firmware → `esp32-12-release`

## Đầu ra
Báo cáo theo mẫu + khung file rỗng (header có comment hợp đồng interface) cho từng component đã chốt.
