# Công cụ gỡ lỗi

Chọn công cụ theo **bằng chứng đang thiếu**, không bật tất cả cùng lúc.

| Đang thiếu | Công cụ |
|---|---|
| Log cơ bản, lý do reset | `idf.py monitor` |
| Tên hàm từ backtrace | `addr2line` / decoder của monitor |
| Trạng thái mọi task lúc crash | core dump |
| Xem biến, bước từng dòng | JTAG + GDB, hoặc gdbstub |
| Ai cấp phát mà không giải phóng | heap tracing |
| Task nào ăn CPU | run-time stats |
| Timing giữa các task/ISR | SystemView / GPIO + oscilloscope |
| Tín hiệu trên dây | logic analyzer / oscilloscope |

---

## idf.py monitor

```
idf.py -p COM5 monitor              # Ctrl+] thoát
idf.py -p COM5 flash monitor
idf.py -p COM5 -b 115200 monitor
```
Tự giải mã backtrace nếu có `esp32_exception_decoder`. Nhấn `Ctrl+T Ctrl+H` để xem phím tắt
(reset board, bật/tắt timestamp, in bộ lọc log).

PlatformIO:
```ini
monitor_speed = 115200
monitor_filters = esp32_exception_decoder, time
```

Bật timestamp khi điều tra lỗi theo thời gian (rò heap, reconnect) — không có mốc thời gian thì
không dựng được đường cong.

### Bộ lọc log lúc chạy
```c
esp_log_level_set("*", ESP_LOG_INFO);
esp_log_level_set("wifi", ESP_LOG_DEBUG);      /* chỉ mở component đang điều tra */
esp_log_level_set("mqtt_client", ESP_LOG_DEBUG);
```
Mở DEBUG toàn cục làm log tràn và **đổi timing** — đủ để che race condition. Mở đúng component.

---

## addr2line

```
xtensa-esp32-elf-addr2line   -pfiaC -e build/app.elf 0x400d1234 0x400d5678
xtensa-esp32s3-elf-addr2line -pfiaC -e build/app.elf 0x42001234
riscv32-esp-elf-addr2line    -pfiaC -e build/app.elf 0x42001234     # C3 / C6
```
ELF phải đúng build đang chạy. Xem `panic-backtrace.md`.

---

## Core dump

```
CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y
CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF=y
CONFIG_ESP_COREDUMP_CHECKSUM_CRC32=y
```
Cần partition `coredump` (`esp32-01-project-init/references/partitions.md`).

```
idf.py coredump-info                 # tóm tắt + backtrace task crash
idf.py coredump-debug                # mở GDB trên core dump
idf.py coredump-erase                # xoá để lần sau ghi dump mới
```
Trong GDB trên core dump:
```
info threads          # mọi task
thread 3              # chuyển sang task 3
bt                    # backtrace của task đó
info locals
p g_state
```
Đây là cách duy nhất xem trạng thái **mọi** task — bắt buộc khi nghi deadlock.

Core dump cũng ghi được ra UART (`..._ENABLE_TO_UART`) nếu không có partition, nhưng phải có người
hứng log lúc crash.

---

## gdbstub (panic → GDB qua serial, không cần JTAG)

```
CONFIG_ESP_SYSTEM_PANIC_GDBSTUB=y
```
Khi panic, chip dừng lại chờ GDB trên chính cổng serial; `idf.py monitor` tự chuyển sang GDB.
Xem được biến và stack tại đúng thời điểm crash.

**Không bật ở firmware production**: thiết bị sẽ đứng chờ GDB thay vì reset và tự phục hồi.

---

## JTAG / OpenOCD / GDB

S3, C3, C6 có USB-Serial-JTAG tích hợp — cắm một sợi USB là debug được, không cần adapter ngoài.
ESP32 classic cần adapter (FT2232H, ESP-Prog...).

```
idf.py openocd                       # chạy OpenOCD
idf.py gdb                           # GDB nối tới OpenOCD
idf.py openocd gdb                   # cả hai
```
Trong GDB:
```
b app_main            # breakpoint
c / n / s             # continue / next / step
watch g_counter       # dừng khi biến bị ghi — công cụ mạnh nhất để bắt heap/biến bị phá
info threads
```
`watch` trên biến bị hỏng thường tìm ra thủ phạm nhanh hơn mọi cách khác khi nghi ghi đè bộ nhớ.

Lưu ý: chân JTAG có thể trùng chân dự án đang dùng — kiểm tra
`esp32-02-hardware-analysis/references/boot-debug-usb.md` trước khi nối.

---

## Heap tracing

```
CONFIG_HEAP_TRACING_STANDALONE=y
CONFIG_HEAP_TRACING_STACK_DEPTH=6
```
```c
#include "esp_heap_trace.h"
static heap_trace_record_t s_rec[128];
heap_trace_init_standalone(s_rec, 128);
heap_trace_start(HEAP_TRACE_LEAKS);
/* ... một chu kỳ nghi ngờ ... */
heap_trace_stop();
heap_trace_dump();
```
Chỉ bọc **đúng đoạn nghi ngờ**, không trace cả chương trình — bộ đệm bản ghi sẽ đầy và mất dữ liệu.
Chi tiết dùng: `memory-faults.md`.

---

## Heap poisoning và kiểm tra toàn vẹn

```
CONFIG_HEAP_POISONING_COMPREHENSIVE=y
```
```c
heap_caps_check_integrity_all(true);
```
Chậm rõ rệt. Chỉ bật khi đang truy heap corruption, **tắt trước khi ship**.

---

## Run-time stats và danh sách task

```
CONFIG_FREERTOS_USE_TRACE_FACILITY=y
CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y
CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID=y
```
```c
char buf[1024];
vTaskGetRunTimeStats(buf);  ESP_LOGI(TAG, "\n%s", buf);
vTaskList(buf);             ESP_LOGI(TAG, "\n%s", buf);
```
`vTaskList` cột `Stack` là high-water mark **còn lại** của từng task — vừa dò tràn stack, vừa dò
task treo (trạng thái `B`locked / `R`unning / `S`uspended).

Buffer phải đủ lớn (~40 byte mỗi task); buffer nhỏ sẽ tràn stack ngay trong lúc debug.

---

## Đo timing bằng GPIO

Khi cần biết ISR hay task chạy mất bao lâu mà không muốn ảnh hưởng timing bằng log:
```c
gpio_set_level(DBG_PIN, 1);
/* đoạn cần đo */
gpio_set_level(DBG_PIN, 0);
```
Xem bằng oscilloscope/logic analyzer. Chính xác hơn log rất nhiều và gần như không nhiễu timing —
đặc biệt hợp để điều tra IWDT và jitter.

---

## SystemView (nâng cao)

Ghi lại chuỗi chuyển ngữ cảnh task/ISR theo thời gian thực. Dùng khi cần thấy **thứ tự** sự kiện
giữa nhiều task — deadlock phức tạp, priority inversion. Cần JTAG và cấu hình
`CONFIG_APPTRACE_SV_ENABLE`. Chi phí thiết lập cao; chỉ dùng khi các cách trên đã bí.

---

## Nguyên tắc dùng công cụ

- Bật **một** công cụ mỗi lần. Bật nhiều thứ cùng lúc làm đổi timing và có thể làm lỗi biến mất.
- Mọi công cụ ở đây đều đổi timing ít nhiều. Lỗi "biến mất khi bật debug" là thông tin quan trọng
  (rất có thể là race condition), **không phải là đã sửa xong**.
- Ghi lại chính xác cấu hình đã bật khi thu thập bằng chứng — số liệu đo với poisoning bật khác
  với lúc ship.
- Tắt hết công cụ debug trước khi ship: `esp32-12-release`.
