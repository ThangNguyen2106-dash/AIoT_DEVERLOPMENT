# Bằng chứng cần có theo từng lớp lỗi

Dùng ở **bước 2 (Evidence)**. Nguyên tắc: thiếu gì hỏi nấy, hỏi **một lô**, kèm đúng lệnh để
người dùng chạy. Không suy đoán thay cho bằng chứng thiếu.

## 0. Bằng chứng nền — luôn cần, mọi lớp lỗi

- [ ] **Log serial đầy đủ**, bắt đầu từ dòng `rst:0x...` (hoặc `ESP-ROM:esp32...`) tới dòng cuối.
- [ ] Chip/module chính xác + framework + phiên bản (`idf.py --version`, hoặc `platformio.ini`).
- [ ] Lỗi **tái hiện được không**: luôn luôn / ngẫu nhiên / chỉ sau N giờ / chỉ khi có tải.
- [ ] **Mới xuất hiện sau thay đổi nào**: commit nào, đổi phần cứng gì, đổi nguồn gì.
- [ ] Nguồn cấp thực tế: USB cổng máy tính / adapter / pin — và có tải lớn (relay, motor) không.

Thiếu log → câu hỏi chuẩn:

> Chạy `idf.py monitor` (hoặc mở serial 115200), nhấn reset, để chạy tới lúc lỗi, rồi dán
> **toàn bộ** từ dòng `rst:0x...` đầu tiên. Đừng cắt phần ký tự rác lúc boot — dòng đó chứa
> lý do reset.

## 1. Boot failure / boot loop

- [ ] Log bootloader đầy đủ (phần trước `app_main`), gồm cả `ESP-ROM:...` và `boot:` .
- [ ] Log có lặp lại y hệt mỗi vòng không, hay khác nhau mỗi lần.
- [ ] Vừa đổi partition table / flash size / sdkconfig / bật encryption hay secure boot không.
- [ ] Đo điện áp 3V3 lúc boot (nếu nghi nguồn).
- [ ] Board có chạy được firmware `hello_world` sạch không → tách phần cứng khỏi phần mềm.

## 2. Panic / Guru Meditation

- [ ] Nguyên văn khối panic: dòng `Guru Meditation Error`, `Core N register dump`, `Backtrace:`.
- [ ] File ELF **của đúng build đang chạy** (`build/<project>.elf`) — để giải mã backtrace.
- [ ] Đã giải mã backtrace chưa; nếu chưa, dán cả dòng `Backtrace: 0x... 0x...`.
- [ ] Panic xảy ra ngay sau hành động cụ thể nào (nhận bản tin, cắm/rút cảm biến, OTA...).

> Nếu ELF đã bị build đè: **phải build lại đúng commit đã nạp**, hoặc coi backtrace là vô giá trị.

## 3. Stack overflow

- [ ] Tên task trong thông báo lỗi.
- [ ] `uxTaskGetStackHighWaterMark(NULL)` in định kỳ trong task nghi ngờ, hoặc `vTaskList()`.
- [ ] Kích thước stack đang cấp khi tạo task (con số thật trong code).
- [ ] Trong task có mảng cục bộ lớn, `sprintf`/`snprintf` buffer, JSON parser, hay TLS không.
- [ ] `CONFIG_FREERTOS_CHECK_STACKOVERFLOW` đang ở mức nào (none/canary/watchpoint).

## 4. Heap corruption / leak / phân mảnh

- [ ] In định kỳ: `esp_get_free_heap_size()`, `esp_get_minimum_free_heap_size()`,
      `heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)` — kèm mốc thời gian.
- [ ] Đường cong free heap theo thời gian (giảm dần đều = rò; răng cưa nhưng đáy tụt = rò chậm).
- [ ] Đã bật `CONFIG_HEAP_POISONING_COMPREHENSIVE` chạy lại chưa.
- [ ] Đã bật `CONFIG_HEAP_TRACING` + `heap_trace_dump()` cho vùng nghi ngờ chưa.
- [ ] Thông báo lỗi nguyên văn (`CORRUPT HEAP: ...`, `heap_caps_free ... was not allocated`).

## 5. Task watchdog (TWDT)

- [ ] Nguyên văn khối TWDT: danh sách task bị `- IDLE`, `- <task>` và dòng `CPU N: <task>`.
- [ ] Backtrace của task bị treo (TWDT có in nếu bật `CONFIG_ESP_TASK_WDT_PANIC`).
- [ ] `vTaskGetRunTimeStats()` chụp lúc gần lỗi — task nào ăn CPU.
- [ ] Priority + core affinity của mọi task trong hệ thống.
- [ ] Trong task có vòng lặp chờ bận, `while(!flag){}`, hay lời gọi block lâu không.

## 6. Interrupt watchdog (IWDT)

- [ ] Nguyên văn `Interrupt wdt timeout on CPU0/CPU1` + register dump.
- [ ] Danh sách ISR đã đăng ký, mỗi ISR làm gì, có gọi hàm nào ở flash không.
- [ ] Có đoạn nào `portENTER_CRITICAL` / tắt ngắt dài không.
- [ ] Có thao tác flash/NVS/OTA chạy đồng thời không (flash write làm treo cache).

## 7. Deadlock / treo im

- [ ] Xác nhận thật sự treo: LED heartbeat có nhấp nháy không, có phản hồi serial không.
- [ ] TWDT có bật không — nếu không bật, treo sẽ **im lặng** và không có log; bật rồi chạy lại.
- [ ] Danh sách mutex/semaphore và thứ tự lấy khoá ở từng task.
- [ ] Có `portMAX_DELAY` ở đâu (đây là nơi treo vĩnh viễn).
- [ ] Nếu có JTAG: chụp `info threads` + `bt` của mọi task lúc treo.

## 8. Race condition / dữ liệu sai thất thường

- [ ] Biến/cấu trúc nào sai, giá trị sai trông như thế nào (nhảy số, lẫn nửa bản ghi...).
- [ ] Những task/ISR nào chạm vào biến đó.
- [ ] Biến có `volatile` không, có khoá không, có phải 64-bit hoặc struct nhiều trường không.
- [ ] Lỗi có thay đổi tần suất khi đổi priority hoặc thêm log không (dấu hiệu race rất mạnh).

## 9. Brownout / reset do nguồn

- [ ] Dòng `Brownout detector was triggered` hoặc `rst:0x10` trong log.
- [ ] Reset có trùng thời điểm bật tải nào không (relay, motor, LED công suất, Wi-Fi TX).
- [ ] Nguồn: loại adapter, dòng định mức, chiều dài và tiết diện dây, cáp USB.
- [ ] Tụ trên chân 3V3 của module (khuyến nghị có tụ lớn gần module).
- [ ] Đo 3V3 bằng oscilloscope lúc bật tải — sụt bao nhiêu, trong bao lâu.

> Brownout là lỗi phần cứng. Không đề xuất tắt brownout detector trong mọi trường hợp.

## 10. Timeout giao tiếp

- [ ] Mã lỗi nguyên văn và hàm trả về nó.
- [ ] Bus nào, tốc độ bao nhiêu, dài dây bao nhiêu, có pull-up ngoài không (I2C).
- [ ] Lỗi ngay từ đầu hay chỉ xuất hiện sau một thời gian chạy.
- [ ] Một thiết bị lỗi hay mọi thiết bị trên bus đều lỗi.
- [ ] Nếu có: ảnh oscilloscope/analyzer của SDA/SCL hoặc TX/RX lúc lỗi.
- [ ] Với mạng: RSSI, log `wifi:` , disconnect reason code, log broker.

## Khi người dùng không cung cấp được bằng chứng

Không lấp bằng suy đoán. Trình bày như sau:

```
## Còn treo — không kết luận được nếu thiếu
1. <bằng chứng cần> — vì nó phân biệt H1 với H2
   Cách lấy: <lệnh / config cụ thể>
```

Trong lúc chờ, được phép làm việc **không phụ thuộc bằng chứng đó**: đọc code tìm ứng viên,
chuẩn bị sẵn instrumentation, thu hẹp danh sách giả thuyết. Ghi rõ đây là hạng 4.
