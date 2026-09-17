# Panic, Guru Meditation, backtrace, core dump

Phục vụ lớp lỗi: crash có panic log (`rst:0xc (SW_CPU_RESET)`).

## Đọc khối panic

Khối panic điển hình gồm bốn phần — đọc đủ cả bốn, đừng chỉ nhìn dòng đầu:

```
Guru Meditation Error: Core  0 panic'ed (LoadProhibited). Exception was unhandled.
Core 0 register dump:
PC      : 0x400d21ac  PS      : 0x00060730  A0      : 0x800d2200  A1      : 0x3ffb1f30
...
EXCVADDR: 0x00000000        <-- địa chỉ gây lỗi
Backtrace: 0x400d21ac:0x3ffb1f30 0x400d2200:0x3ffb1f50 ...
```

| Trường | Ý nghĩa |
|---|---|
| `PC` | lệnh đang chạy khi lỗi → nơi cần nhìn đầu tiên |
| `EXCVADDR` | địa chỉ bị truy cập sai — `0x0` nghĩa là NULL deref; giá trị rác nghĩa là con trỏ hỏng |
| `A1` | con trỏ stack — so với dải stack của task để biết có tràn không |
| `Backtrace` | chuỗi `PC:SP` — chuỗi gọi hàm |
| `Core N` | crash trên lõi nào (ESP32/S3 hai lõi) |

## Bảng nguyên nhân theo loại exception

| Loại | Nghĩa | Nguyên nhân thường gặp |
|---|---|---|
| `LoadProhibited` | đọc từ địa chỉ không hợp lệ | dereference NULL, con trỏ đã `free`, struct chưa init |
| `StoreProhibited` | ghi vào địa chỉ không hợp lệ | ghi qua NULL, ghi vào `const`/flash, con trỏ hỏng |
| `LoadStoreAlignment` | truy cập không đúng biên | ép kiểu con trỏ sai, đọc `uint32_t` từ buffer lệch byte |
| `IllegalInstruction` | nhảy vào vùng không phải lệnh | con trỏ hàm rác, callback đã free, stack bị ghi đè, gọi hàm ở flash khi cache tắt |
| `InstrFetchProhibited` | fetch lệnh từ vùng cấm | gọi qua con trỏ hàm NULL/rác |
| `Cache disabled but cached memory region accessed` | chạm flash khi cache tắt | ISR hoặc callback không có `IRAM_ATTR`, hằng chuỗi ở flash trong ISR, chạy trong lúc ghi flash |
| `abort() was called` | `abort`/assert chủ động | `ESP_ERROR_CHECK` thất bại, assert của thư viện, `configASSERT` |
| `assert failed: <hàm> <file>:<line>` | assert nội bộ IDF/FreeRTOS | dòng này **đã chỉ thẳng file:line** — đọc nó trước khi làm gì khác |
| `Stack canary watchpoint triggered (task)` | tràn stack | xem `memory-faults.md` |
| `Unhandled debug exception` / `Debug exception reason: Stack canary` | tràn stack | xem `memory-faults.md` |

`EXCVADDR = 0x00000000` gần như luôn là NULL pointer. `EXCVADDR` gần `0x3ff...`/`0x4xx...` nhưng
lệch vài byte thường là struct hỏng hoặc offset sai trên con trỏ hợp lệ.

## Giải mã backtrace

Backtrace chỉ có giá trị khi giải mã bằng **đúng file ELF của firmware đang chạy**. ELF khác build
sẽ cho tên hàm sai — tệ hơn là không giải mã, vì nó dẫn điều tra đi sai hướng.

```
idf.py monitor                       # tự giải mã nếu bật esp32_exception_decoder
```

Giải mã thủ công:

```
xtensa-esp32-elf-addr2line -pfiaC -e build/app.elf 0x400d21ac 0x400d2200      # ESP32
xtensa-esp32s3-elf-addr2line -pfiaC -e build/app.elf 0x42001234                # S2/S3
riscv32-esp-elf-addr2line -pfiaC -e build/app.elf 0x42001234                   # C3/C6
```

PlatformIO: ELF nằm ở `.pio/build/<env>/firmware.elf`, bật decoder bằng
`monitor_filters = esp32_exception_decoder`.

Trên RISC-V (C3/C6) backtrace không phải lúc nào cũng dựng được đầy đủ do không có window ABI —
nếu backtrace cụt, dùng core dump hoặc gdbstub thay vì cố suy đoán từ một frame.

**Kiểm tra trước khi tin backtrace:**
- [ ] ELF sinh ra từ đúng commit đã nạp (đối chiếu thời gian build, hoặc git hash nhúng trong app desc).
- [ ] Không build lại sau khi nạp.
- [ ] Địa chỉ nằm trong dải hợp lệ; địa chỉ như `0xa5a5a5a5` hoặc `0x00000000` là stack đã hỏng,
      khi đó backtrace vô nghĩa → chuyển hướng sang tràn stack / heap corruption.

## Từ backtrace tới nguyên nhân

Backtrace cho biết **ở đâu**, không cho biết **vì sao**. Quy trình tiếp:

1. Mở đúng `file:line` của frame trên cùng.
2. Xác định con trỏ/biến nào ở dòng đó có thể không hợp lệ.
3. Truy ngược: ai cấp phát nó, ai giải phóng, ai gán NULL, có task khác chạm vào không.
4. Nếu con trỏ đến từ heap → nghi `use-after-free` → `memory-faults.md`.
5. Nếu frame trên cùng là hàm thư viện (`malloc`, `memcpy`, `strlen`) → **lỗi không nằm ở đó**,
   mà ở kẻ gọi truyền tham số hỏng. Đọc frame tiếp theo.

Crash "ngẫu nhiên ở những chỗ khác nhau mỗi lần" là dấu hiệu hỏng bộ nhớ (stack hoặc heap),
không phải bug ở nơi crash. Chuyển sang `memory-faults.md` thay vì săn từng vị trí crash.

## `ESP_ERROR_CHECK` và abort

```
ESP_ERROR_CHECK failed: esp_err_t 0x105 (ESP_ERR_NOT_FOUND) at 0x400d1234
file: "main/app_main.c" line 42
func: app_main
expression: nvs_flash_init()
```

Khối này đã cho đủ: mã lỗi, biểu thức, vị trí. Tra mã lỗi:

```
python $IDF_PATH/tools/idf.py --help   # hoặc:
esp_err_to_name(err)                   /* in tên trong code */
```

`ESP_ERROR_CHECK` không phải bug — nó là assert đang làm đúng việc. Bug là **vì sao lời gọi đó
thất bại**. Không "sửa" bằng cách đổi `ESP_ERROR_CHECK` thành bỏ qua lỗi.

## Core dump — bắt buộc cho thiết bị ngoài thực địa

Serial không phải lúc nào cũng cắm được. Core dump lưu trạng thái mọi task lúc panic vào flash.

```
CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y
CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF=y
CONFIG_ESP_COREDUMP_CHECKSUM_CRC32=y
```

Cần một partition tên `coredump` trong partition table (xem `esp32-01-project-init`).

```
idf.py coredump-info                 # tóm tắt: task nào crash, backtrace
idf.py coredump-debug                # mở GDB trên core dump: xem biến, stack mọi task
esp-coredump info_corefile -t raw -c coredump.bin build/app.elf
```

Core dump hơn hẳn backtrace: xem được **mọi task**, không chỉ task crash — cần thiết khi nghi
deadlock hoặc nghi task khác mới là thủ phạm.

Đọc core dump lấy từ thiết bị đã ship: `esp32-12-release`.

## gdbstub — bắt trạng thái ngay lúc panic mà không cần JTAG

```
CONFIG_ESP_SYSTEM_PANIC_GDBSTUB=y
```

Khi panic, chip không reset mà chờ GDB qua chính cổng serial. Kết nối bằng `idf.py monitor`
(tự chuyển sang GDB) rồi `bt`, `info locals`, `p <biến>`. Rất hiệu quả cho crash khó tái hiện.

Không bật ở firmware production: thiết bị sẽ **đứng im chờ GDB** thay vì reset và tự phục hồi.

## Nếu panic xảy ra trong ISR

Register dump ghi rõ đang ở ngữ cảnh ngắt. Nguyên nhân hay gặp:
- Gọi hàm không an toàn trong ISR (`ESP_LOGI`, `malloc`, hàm không có hậu tố `FromISR`).
- Gọi hàm nằm ở flash khi cache tắt → thiếu `IRAM_ATTR` (cả hàm lẫn hằng dữ liệu).
- Dùng `vTaskDelay`/mutex trong ISR.

Thiết kế lại ISR cho đúng: `esp32-03-firmware-architecture`.
