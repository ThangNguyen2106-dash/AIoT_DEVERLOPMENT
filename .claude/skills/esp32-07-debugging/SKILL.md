---
name: esp32-07-debugging
description: Gỡ lỗi firmware ESP32 bằng phân tích nguyên nhân gốc — crash, boot loop, watchdog reset (TWDT/IWDT), Guru Meditation, stack overflow, heap corruption, memory leak, brownout, deadlock, starvation, race condition, timeout giao tiếp, boot failure. Giải mã backtrace, core dump, reset reason, JTAG/GDB, heap trace. Dùng khi thiết bị crash, treo, reset ngẫu nhiên, không boot, hoặc hỏng sau vài giờ chạy.
---

# 07 — Debugging

## Điều kiện vào
Chưa rõ chip / framework / phiên bản IDF → chạy Context Gate ở `esp32-firmware` trước.
Cổng an toàn và ngân sách context của `esp32-firmware` áp dụng nguyên vẹn tại đây.

Phản ứng với lỗi **đã xảy ra**. Skill này **điều tra**, không thiết kế lại và không tối ưu.
Đầu ra là một **báo cáo RCA** (nguyên nhân gốc + bản vá tối thiểu + test chặn hồi quy),
không phải một loạt thay đổi thử-xem-sao.

## Nguyên tắc tối thượng: nguyên nhân trước, bản vá sau

Firmware nhúng chạy hàng tháng không ai nhìn. Một bản vá che triệu chứng sẽ quay lại ngoài
thực địa, ở nơi không cắm được serial. **Không đề xuất bất kỳ thay đổi nào trước khi mệnh đề
nguyên nhân gốc được xác minh.**

### Danh sách cấm — không bao giờ đề xuất như bản sửa khi chưa có Root Cause

| Hành vi bị cấm | Vì sao |
|---|---|
| Thêm `delay()` / `vTaskDelay()` cho "hết lỗi" | che race condition; lỗi quay lại khi tải thay đổi |
| Tăng timeout watchdog, hoặc rải `esp_task_wdt_reset()` trong vòng lặp | tắt còi báo cháy, không dập lửa |
| `esp_task_wdt_delete()` / tắt TWDT / tắt IWDT | mất luôn khả năng phát hiện treo ngoài thực địa |
| Phóng to stack "cho chắc" | giấu tràn stack thật; hết RAM ở chỗ khác |
| `esp_restart()` định kỳ, hoặc auto-reboot khi gặp lỗi | rò bộ nhớ vẫn còn, chỉ giãn chu kỳ crash |
| Tắt brownout detector | brownout là lỗi **nguồn**; tắt đi thì chip chạy ở vùng điện áp không đảm bảo, có thể hỏng nội dung flash |
| Nuốt `esp_err_t` để "không crash nữa" | biến crash thành dữ liệu sai âm thầm — tệ hơn crash |
| Đổi `portMAX_DELAY` thành timeout hữu hạn **để thoát một vụ treo chưa có root cause** | che deadlock, không gỡ deadlock. Loại bỏ `portMAX_DELAY` ở **code mới** là bắt buộc theo `esp32-04` / `esp32-05` — đó là thiết kế, không phải bản vá |
| Hạ tốc bus (I2C 400k → 100k) khi chưa đo | có thể đúng, nhưng chưa đo thì chỉ là may rủi |
| Thêm retry quanh một lời gọi hay thất bại | che lỗi cấu hình/phần cứng bên dưới |

Khi người dùng đang vội và muốn chạy tạm: được phép nêu bản vá tạm, nhưng **phải gắn nhãn
`[VÁ TẠM]`, nói rõ nó che cái gì, và vẫn tiếp tục truy nguyên nhân gốc.** Không trình bày vá tạm
như thể đã sửa xong.

## Quy trình 7 bước (bắt buộc, không nhảy bước)

```
Symptom → Evidence → Hypothesis → Verification → Root Cause → Minimal Fix → Regression Test
```

| # | Bước | Ra khỏi bước khi | Cấm |
|---|---|---|---|
| 1 | **Symptom** | mô tả được: xảy ra khi nào, tần suất, tái hiện được không, mới xuất hiện sau thay đổi nào | không nhận "nó bị lỗi" làm symptom |
| 2 | **Evidence** | đã có đủ bằng chứng hạng 1–2 cho lớp lỗi này (`checklists/evidence-request.md`) | **không đoán thay cho log** |
| 3 | **Hypothesis** | có 1–3 giả thuyết cụ thể, mỗi giả thuyết **bác bỏ được** | không nêu giả thuyết kiểu "chắc do nhiễu" |
| 4 | **Verification** | mỗi giả thuyết đã được xác nhận hoặc bác bỏ bằng một phép đo / thí nghiệm | không xác minh bằng "sửa thử thấy hết lỗi" |
| 5 | **Root Cause** | giải thích được **toàn bộ** triệu chứng, kể cả tần suất và điều kiện xuất hiện | còn triệu chứng chưa giải thích được ⇒ chưa phải root cause |
| 6 | **Minimal Fix** | thay đổi nhỏ nhất triệt tiêu nguyên nhân, kèm lý do vì sao nó triệt tiêu | không refactor kèm; không sửa 5 thứ cùng lúc |
| 7 | **Regression Test** | có test/kịch bản phát hiện lại lỗi nếu nó tái xuất | không kết thúc bằng "đã test tay, thấy ổn" |

Sửa nhiều thứ cùng lúc thì mất luôn khả năng quy kết: hết lỗi cũng không biết nhờ cái nào.
Bước 4 không xác minh được vì thiếu phần cứng/quyền truy cập → **nói rõ giả thuyết nào còn treo**,
không nâng giả thuyết lên thành kết luận.

## Thang bằng chứng

| Hạng | Nguồn | Dùng được để |
|---|---|---|
| 1 | Log serial đầy đủ từ dòng `rst:0x...`, core dump, backtrace giải mã đúng ELF | kết luận |
| 2 | Số đo: `heap_caps_*`, stack high-water mark, `vTaskGetRunTimeStats`, oscilloscope, đồng hồ dòng | kết luận |
| 3 | Tái hiện có kiểm soát (bật/tắt từng task, giảm tải, đổi board, đổi nguồn) | thu hẹp |
| 4 | Đọc code và suy luận | nêu **giả thuyết**, không phải kết luận |
| 5 | Kinh nghiệm chung / "thường là do..." | **không được dùng làm căn cứ kết luận** |

Chỉ có hạng 4–5 → dừng, hỏi đúng thứ cần (`checklists/evidence-request.md`).
Log bị cắt đầu, thiếu dòng `rst:` → **coi như chưa có log**, xin lại.
Backtrace giải mã bằng ELF không khớp firmware đang chạy → **bằng chứng rác**, tên hàm sẽ sai.

## Thiếu log thì hỏi, không đoán

Câu hỏi phải **cụ thể và thi hành được**: nêu đúng lệnh cần chạy, đúng config cần bật, đúng đoạn
log cần dán.

- Đạt: *"Chạy `idf.py monitor`, nhấn nút reset, dán toàn bộ từ dòng `rst:0x...` tới dòng cuối
  trước khi treo — giữ cả phần ký tự rác lúc boot."*
- Không đạt: *"Bạn gửi log nhé."*

Hỏi **theo lô**: gom mọi thứ cần vào một lượt, không hỏi nhỏ giọt từng vòng.
Mẫu câu hỏi theo từng lớp lỗi: `checklists/evidence-request.md`.

## Định tuyến theo lớp lỗi

Mỗi lớp lỗi chỉ đọc **một** reference tương ứng. Không quét cả thư mục.

| Triệu chứng | Lớp lỗi | Reference |
|---|---|---|
| Không boot, treo ở bootloader, boot loop trước `app_main` | boot failure | `references/boot-and-reset.md` |
| Reset ngẫu nhiên, cần biết vì sao reset | reset reason | `references/boot-and-reset.md` |
| `Brownout detector was triggered`, reset đúng lúc bật tải | brownout | `references/boot-and-reset.md` |
| `Guru Meditation Error: ...`, `abort()`, `assert failed` | panic | `references/panic-backtrace.md` |
| Backtrace khó đọc, cần core dump / gdbstub | giải mã | `references/panic-backtrace.md` |
| `stack overflow in task`, `Stack canary watchpoint` | stack | `references/memory-faults.md` |
| `CORRUPT HEAP`, crash bên trong `malloc`/`free` | heap corruption | `references/memory-faults.md` |
| Free heap giảm dần, crash sau vài giờ/vài ngày | memory leak | `references/memory-faults.md` |
| Free heap còn nhiều nhưng cấp phát khối lớn thất bại | phân mảnh | `references/memory-faults.md` |
| `Task watchdog got triggered` | TWDT | `references/rtos-faults.md` |
| `Interrupt wdt timeout on CPU0/1` | IWDT | `references/rtos-faults.md` |
| Treo im, không log, không watchdog | deadlock | `references/rtos-faults.md` |
| Task chạy trễ hoặc không chạy, hệ thống giật | starvation / priority inversion | `references/rtos-faults.md` |
| Dữ liệu sai thất thường, giá trị nhảy vô lý | race condition | `references/rtos-faults.md` |
| `ESP_ERR_TIMEOUT`, I2C/SPI/UART/Modbus không phản hồi | timeout ngoại vi | `references/comm-timeouts.md` |
| Mất Wi-Fi/MQTT rồi không tự phục hồi | timeout mạng | `references/comm-timeouts.md` |
| Cần bật công cụ: monitor, core dump, JTAG, heap trace, SystemView | công cụ | `references/tooling.md` |

Nhiều triệu chứng cùng lúc → điều tra **cái sớm nhất theo thời gian**; các triệu chứng sau
thường là hệ quả (ví dụ: rò heap → cấp phát thất bại → NULL deref → panic).

## Mẫu báo cáo (bám sát, không viết dài)

```
## Symptom
<khi nào, tần suất, tái hiện được không, xuất hiện sau thay đổi nào>

## Evidence
[Hạng 1] rst:0xc (SW_CPU_RESET) + Guru Meditation LoadProhibited, PC 0x400d21ac
[Hạng 2] free heap 142k → 38k sau 6 giờ, largest block 31k
[THIẾU]  chưa có high-water mark của mqtt_task

## Hypothesis
H1 — payload MQTT bị free hai lần trong nhánh lỗi     → bác bỏ được bằng heap poisoning
H2 — buffer 4KB khai báo cục bộ trong mqtt_task (stack 3KB) → bác bỏ được bằng high-water mark

## Verification
H1: bật CONFIG_HEAP_POISONING_COMPREHENSIVE, chạy lại → không báo → BÁC BỎ
H2: uxTaskGetStackHighWaterMark = 48 byte ngay trước crash → XÁC NHẬN

## Root Cause
mqtt_task cấp stack 3072 byte nhưng nhánh xử lý bản tin cấu hình khai báo
`char json[4096]` trên stack. Chỉ tràn khi server gửi bản tin cấu hình — giải thích
đúng việc lỗi hiếm và không tái hiện được trên bàn.
File: main/mqtt_task.c:118

## Minimal Fix
Chuyển `json[4096]` sang heap (malloc + free), giữ nguyên stack 3072.
Vì sao triệt tiêu: khung stack lớn nhất không còn phụ thuộc kích thước bản tin.
[VÁ TẠM nếu phải chạy gấp] tăng stack lên 8192 — che được lần này, vẫn tràn nếu bản tin lớn hơn.

## Regression Test
Test gửi bản tin cấu hình 8KB; assert high-water mark > 512 byte.
→ chuyển esp32-08-testing hiện thực.

## Còn treo
<giả thuyết chưa xác minh được, hoặc bằng chứng còn thiếu>
```

## Không thuộc scope
- Thiết kế lại task/khoá/phân lớp để lỗi không tái diễn → `esp32-03-firmware-architecture`
- Hiện thực test chặn hồi quy → `esp32-08-testing`
- Tối ưu RAM/stack/tốc độ sau khi đã đúng → `esp32-09-performance-optimization`
- Nghi lỗi từ pin map / điện áp / nguồn ở mức thiết kế → `esp32-02-hardware-analysis`
- Mất kết nối do state machine reconnect thiếu → `esp32-05-connectivity`
- Bật core dump trên thiết bị đã ship, chẩn đoán từ xa → `esp32-12-release`

## References
- `checklists/evidence-request.md` — cần bằng chứng gì cho từng lớp lỗi, câu hỏi mẫu
- `checklists/rca-checklist.md` — cổng kiểm tra 7 bước trước khi kết luận
- `references/boot-and-reset.md` — reset reason, boot failure, bootloader log, brownout
- `references/panic-backtrace.md` — Guru Meditation, abort/assert, addr2line, core dump
- `references/memory-faults.md` — stack overflow, heap corruption, leak, phân mảnh
- `references/rtos-faults.md` — TWDT, IWDT, deadlock, starvation, priority inversion, race
- `references/comm-timeouts.md` — timeout I2C/SPI/UART/Modbus/mạng, khoanh vùng lỗi ở đâu
- `references/tooling.md` — monitor, core dump, JTAG/GDB, gdbstub, heap trace, runtime stats
- `templates/rca-report.md` — mẫu báo cáo RCA đầy đủ

## Đầu ra
Báo cáo RCA theo mẫu + bản vá tối thiểu + mô tả test chặn hồi quy.
Chưa xác minh được nguyên nhân thì đầu ra là **danh sách bằng chứng còn thiếu**, không phải bản vá.
