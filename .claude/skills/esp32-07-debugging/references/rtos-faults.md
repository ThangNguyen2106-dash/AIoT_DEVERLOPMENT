# Watchdog, deadlock, starvation, priority inversion, race condition

Phục vụ lớp lỗi: TWDT, IWDT, treo im, task không chạy, dữ liệu sai thất thường.

## Bốn triệu chứng, bốn hướng khác nhau

| Triệu chứng | Lớp lỗi | Điểm nhận dạng |
|---|---|---|
| `Task watchdog got triggered` | TWDT | một task không nhường CPU đủ lâu |
| `Interrupt wdt timeout on CPU0/1` | IWDT | ngắt bị chặn quá lâu — nghiêm trọng hơn TWDT |
| Treo hoàn toàn, không log, không reset | deadlock hoặc chờ vĩnh viễn | không có watchdog nào bật |
| Task chạy trễ / bỏ nhịp, hệ thống giật | starvation / priority inversion | task priority thấp bị bỏ đói |
| Giá trị sai thất thường | race condition | tần suất đổi khi thêm log hoặc đổi priority |

---

## 1. Task watchdog (TWDT)

### Đọc log
```
E (5234) task_wdt: Task watchdog got triggered. The following tasks did not reset the watchdog in time:
E (5234) task_wdt:  - IDLE0 (CPU 0)
E (5234) task_wdt: Tasks currently running:
E (5234) task_wdt: CPU 0: sensor_task
```
Hai dòng này nói hai chuyện khác nhau:
- **"did not reset"**: task nào đang bị theo dõi mà không kịp báo cáo. `IDLE0` bị liệt kê nghĩa là
  idle task của lõi 0 không được chạy → có task nào đó chiếm CPU lõi 0.
- **"currently running"**: **thủ phạm** — task đang giữ CPU tại thời điểm watchdog kêu.

Nhìn dòng "currently running" trước.

### Lấy thêm bằng chứng
```
CONFIG_ESP_TASK_WDT_PANIC=y        # biến TWDT thành panic → có backtrace của task treo
CONFIG_FREERTOS_USE_TRACE_FACILITY=y
CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y
```
```c
char buf[1024];
vTaskGetRunTimeStats(buf);   ESP_LOGI(TAG, "\n%s", buf);   /* % CPU theo task */
vTaskList(buf);              ESP_LOGI(TAG, "\n%s", buf);   /* trạng thái + stack còn lại */
```
Cột `%` trong run-time stats chỉ thẳng task đang ăn CPU.

### Nguyên nhân thường gặp
| Nguyên nhân | Dấu hiệu |
|---|---|
| Vòng lặp bận không có `vTaskDelay` | task đó chiếm ~100% CPU trong run-time stats |
| Chờ cờ kiểu `while(!flag){}` | treo tới khi ISR đặt cờ; nếu ISR không chạy thì treo mãi |
| Task priority cao chạy liên tục, không block | task priority thấp và IDLE không bao giờ chạy |
| Thao tác chặn dài: ghi flash/NVS lớn, TLS handshake, `fopen` trên SD | tất định, đúng lúc làm việc đó |
| `taskYIELD()` thay cho block thật | vẫn không nhường cho task priority thấp hơn |
| Chờ mutex bị giữ bởi task khác | thực ra là deadlock/inversion, xem mục 3 và 4 |

### Minimal Fix
Làm cho task **block thật sự**: chờ queue/semaphore/notification với timeout, hoặc `vTaskDelay`
trong vòng lặp có chu kỳ. Chia thao tác dài thành nhiều bước. Đẩy việc nặng sang task riêng
priority thấp hơn.

**Cấm:** `esp_task_wdt_delete()`, nới `CONFIG_ESP_TASK_WDT_TIMEOUT_S`, hay rải
`esp_task_wdt_reset()` giữa vòng lặp bận. Những cách đó giữ nguyên việc task chiếm CPU, chỉ làm
hệ thống im tiếng.

Chỉ nới timeout khi chứng minh được thao tác đó **bản chất phải mất từng đó thời gian** và không
chia nhỏ được — kèm số đo, và phải ghi lại lý do trong code.

---

## 2. Interrupt watchdog (IWDT)

`Interrupt wdt timeout on CPU0` nghĩa là ngắt bị chặn lâu hơn ngưỡng. Nghiêm trọng hơn TWDT vì nó
phá cả timing hệ thống.

| Nguyên nhân | Chi tiết |
|---|---|
| ISR chạy quá lâu | ISR phải ngắn: đọc cờ, đẩy vào queue, thoát |
| `portENTER_CRITICAL` giữ quá lâu | critical section phải tính bằng micro giây |
| Tắt ngắt thủ công rồi quên bật | kiểm tra mọi đường thoát của hàm |
| ISR gọi hàm nằm ở flash khi cache tắt | thiếu `IRAM_ATTR`; xem `panic-backtrace.md` |
| Ghi flash/NVS/OTA kéo dài | cache tắt trong lúc ghi; ISR chạm flash sẽ chết |
| ISR gọi hàm không an toàn (`ESP_LOGI`, `malloc`, API không `FromISR`) | vừa chậm vừa sai ngữ cảnh |

### Minimal Fix
Rút ngắn ISR: chỉ `xQueueSendFromISR` / `vTaskNotifyGiveFromISR` rồi thoát. Đặt `IRAM_ATTR` cho ISR
và mọi hàm nó gọi. Thu hẹp critical section xuống đúng vài dòng cần bảo vệ.

Thiết kế ISR/task chuẩn: `esp32-03-firmware-architecture`.

---

## 3. Deadlock và chờ vĩnh viễn

### Nhận dạng
Thiết bị đứng im: không log, không reset, LED heartbeat ngừng. **Không có watchdog nào bật** thì
lỗi này im lặng tuyệt đối.

Việc đầu tiên: **bật TWDT cho các task chính** để biến treo im thành log có tên task.

### Nguyên nhân
| Dạng | Mô tả |
|---|---|
| Deadlock hai khoá | task A giữ M1 chờ M2, task B giữ M2 chờ M1 |
| `portMAX_DELAY` | chờ một sự kiện không bao giờ tới (peer chết, ISR không kích hoạt) |
| Lấy mutex hai lần trong cùng task | mutex thường không đệ quy — dùng `xSemaphoreCreateRecursiveMutex` nếu thật sự cần |
| Lấy mutex trong ISR | sai ngữ cảnh, treo hoặc panic |
| Queue đầy, nhà sản xuất chờ vô hạn; nhà tiêu thụ đang chờ nhà sản xuất | vòng chờ qua queue |
| Chờ event group bit không bao giờ được set | nhánh lỗi quên set bit |

### Xác minh
- Core dump hoặc JTAG: xem **mọi task** đang block ở đâu.
  ```
  idf.py coredump-debug        # rồi: info threads / thread N / bt
  idf.py openocd gdb           # JTAG: bắt trạng thái ngay lúc treo
  ```
- Không có JTAG: log "đang chờ khoá X" **trước** mỗi lần lấy khoá, log "đã lấy được" ngay sau.
  Dòng cuối trong log chỉ thẳng chỗ treo.

### Minimal Fix
- Áp đặt **thứ tự lấy khoá toàn cục** — mọi task lấy khoá theo cùng một thứ tự thì không thể
  deadlock hai khoá.
- Giảm phạm vi khoá: không gọi hàm chặn (mạng, flash) khi đang giữ mutex.
- Gộp dữ liệu được bảo vệ để chỉ cần **một** khoá.

**Cấm:** đổi `portMAX_DELAY` thành timeout rồi coi là xong. Timeout chỉ biến treo thành lỗi định
kỳ; nó hợp lệ như một **cơ chế phát hiện** (log ra rồi báo lỗi), không phải bản sửa deadlock.

---

## 4. Starvation và priority inversion

### Starvation
Task priority thấp không bao giờ được chạy vì task priority cao luôn sẵn sàng. Triệu chứng: một
tính năng "thỉnh thoảng không chạy", log của task đó thưa dần, IDLE gần 0% trong run-time stats.

Xác minh bằng `vTaskGetRunTimeStats()`: task priority cao chiếm gần hết CPU.

Minimal Fix: làm task priority cao block thật (chờ sự kiện thay vì polling), hoặc hạ priority của
nó. Không sửa bằng cách nâng priority task bị đói — nâng hết thì lại trở về không có ưu tiên.

### Priority inversion
Task priority cao chờ mutex đang bị task priority thấp giữ; task priority trung bình chạy chen vào
làm task priority thấp không chạy được → task priority cao bị chặn vô thời hạn.

FreeRTOS mutex có kế thừa priority (`xSemaphoreCreateMutex`), **binary semaphore thì không**.
Dùng binary semaphore để bảo vệ tài nguyên dùng chung là nguyên nhân inversion phổ biến.

Minimal Fix: dùng đúng mutex (không phải binary semaphore) cho bảo vệ tài nguyên; rút ngắn thời
gian giữ khoá của task priority thấp.

### Core affinity
Trên ESP32/S3 hai lõi, task ghim vào một lõi có thể đói dù lõi kia rảnh. Kiểm tra tham số
`xCoreID` khi tạo task. Phân bổ lõi thuộc `esp32-03-firmware-architecture`.

---

## 5. Race condition

### Nhận dạng
Giá trị sai thất thường, lẫn nửa bản ghi cũ nửa mới, biến đếm nhảy cóc, cấu trúc đọc ra không nhất
quán. **Dấu hiệu rất mạnh:** tần suất lỗi thay đổi khi thêm log, đổi priority, hoặc đổi tốc độ —
vì những thứ đó đổi timing.

### Nguyên nhân
| Dạng | Chi tiết |
|---|---|
| Biến chia sẻ không khoá | task + task, hoặc task + ISR |
| Thiếu `volatile` cho biến ISR chạm vào | compiler tối ưu bỏ lần đọc lại |
| `volatile` nhưng tưởng là đủ | `volatile` **không** tạo tính nguyên tử, chỉ chặn tối ưu |
| Đọc/ghi struct nhiều trường hoặc biến 64-bit | không nguyên tử, đọc được trạng thái nửa vời |
| Read-modify-write (`x++`) | ba thao tác, bị chen giữa chừng |
| Dùng dữ liệu trong khi task khác đang ghi đè buffer | thiếu double-buffer hoặc khoá |

### Xác minh
- Thêm assert bất biến ở nơi đọc (ví dụ: checksum của struct, hoặc kiểm tra dải hợp lệ) — bắt
  được thời điểm dữ liệu không nhất quán.
- Tạm thời bao toàn bộ vùng nghi bằng một mutex: hết lỗi ⇒ **xác nhận đúng là race** (đây là
  bằng chứng, chưa phải bản sửa — bản sửa phải thu hẹp lại đúng vùng cần).
- Đổi priority/affinity để đổi timing: tần suất đổi rõ rệt ⇒ race.

### Minimal Fix
| Dữ liệu | Cách đúng |
|---|---|
| Cờ đơn giản task ↔ ISR | `volatile` + kiểu nguyên tử, hoặc task notification |
| Bộ đếm | `portENTER_CRITICAL` ngắn, hoặc atomic |
| Struct / bản ghi | mutex, hoặc queue (chuyển **bản sao**, không chuyển con trỏ dùng chung) |
| Luồng dữ liệu ISR → task | `xQueueSendFromISR` / `vTaskNotifyGiveFromISR` |
| Buffer lớn | double buffer + trao quyền sở hữu qua queue |

Nguyên tắc: **truyền dữ liệu qua queue thay vì chia sẻ bộ nhớ** loại bỏ cả lớp lỗi này.
Thiết kế lại cơ chế đồng bộ: `esp32-03-firmware-architecture`.

---

## Bộ công tắc nên bật khi điều tra lỗi RTOS

```
CONFIG_FREERTOS_USE_TRACE_FACILITY=y
CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y
CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID=y
CONFIG_ESP_TASK_WDT_PANIC=y                # TWDT kèm backtrace
CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK=y  # loại trừ tràn stack khỏi danh sách nghi
```
Tất cả đều tốn hiệu năng/RAM — **tắt lại trước khi ship**.
