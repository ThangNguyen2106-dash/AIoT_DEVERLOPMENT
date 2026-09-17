---
name: esp32-02-hardware-analysis
description: Thẩm định phần cứng ESP32 trước khi viết firmware — chọn dòng SoC, lập pin map, xung đột GPIO, chân strapping/boot, xung đột ngoại vi (UART/I2C/SPI/LEDC/MCPWM/TWAI/USB/I2S/RMT), ADC/RTC/interrupt, ràng buộc Flash và PSRAM, mức điện áp, pull-up/pull-down, rủi ro nguồn. Dùng khi chọn chip, gán chân, thẩm định schematic, hoặc khi nghi sự cố đến từ phần cứng chứ không phải code.
---

# 02 — Hardware Analysis

## Điều kiện vào
Chưa rõ chip / framework / phiên bản IDF → chạy Context Gate ở `esp32-firmware` trước.
Cổng an toàn và ngân sách context của `esp32-firmware` áp dụng nguyên vẹn tại đây.

Quyết định **cái gì khả thi trên board này**, trước khi bất kỳ skill nào sinh code.
Đầu ra là một pin map đã thẩm định + danh sách rủi ro, không phải code.

## Nguyên tắc tối thượng: không đoán

Phân tích phần cứng sai gây hỏng board, brick chip, hoặc lỗi chập chờn không tìm ra.
**Mọi kết luận phải truy được về một nguồn bằng chứng.** Không có bằng chứng thì không kết luận.

Thứ tự tin cậy của nguồn bằng chứng:

| Hạng | Nguồn | Ghi chú |
|---|---|---|
| 1 | Schematic / board file của chính board đang dùng | cao nhất — nói đúng board này nối gì vào đâu |
| 2 | Datasheet + Technical Reference Manual của SoC | quyết định năng lực chip |
| 3 | Board definition (`platformio.ini` board, `board_config.h`, pin header của nhà sản xuất) | tin được nếu đúng board |
| 4 | Code đang có trong repo | chỉ nói lên **ý định**, không chứng minh phần cứng đúng |
| 5 | Suy đoán từ tên board / kinh nghiệm chung | **KHÔNG được dùng làm căn cứ kết luận** |

Khi bằng chứng chỉ ở hạng 4–5, hoặc thiếu hoàn toàn: **dừng phần đó và hỏi**.
Không tự ý thay đổi pin mapping đang có — đề xuất, nêu lý do, để người dùng quyết định.

## Quy trình

1. **Thu thập** — xác định SoC/module chính xác (ESP32-WROOM-32E khác ESP32-S3-WROOM-1-N16R8),
   board, schematic, danh sách ngoại vi cần dùng, pin map hiện có (nếu có).
2. **Xác minh năng lực chip** — mọi ngoại vi dự án cần có tồn tại trên dòng chip này không?
   (`references/chip-matrix.md`)
3. **Chạy 10 hạng mục kiểm tra** — `checklists/analysis-checklist.md`. Mỗi hạng mục cho kết quả
   PASS / FAIL / UNKNOWN. **UNKNOWN không được làm tròn thành PASS.**
4. **Báo cáo** theo mẫu dưới. Ngắn gọn, có cấu trúc, mỗi phát hiện kèm nguồn bằng chứng.

## 10 hạng mục kiểm tra bắt buộc

| # | Hạng mục | Reference |
|---|---|---|
| 1 | GPIO conflict — hai chức năng cùng một chân, hoặc chân đã bị board chiếm | `pin-constraints.md` |
| 2 | Boot/strapping restriction — chân ảnh hưởng chế độ boot và điện áp flash | `boot-debug-usb.md` |
| 3 | Peripheral conflict — hết instance, trùng timer/DMA/host, ràng buộc IOMUX | `peripheral-matrix.md` |
| 4 | Voltage compatibility — mức logic, thiết bị 5V, chân nguồn, điện áp flash | `power-electrical.md` |
| 5 | Input/output capability — chân input-only, open-drain, drive strength | `pin-constraints.md` |
| 6 | Pull-up/pull-down — có pull nội không, board đã có pull ngoài chưa, bus I2C | `pin-constraints.md` |
| 7 | Interrupt capability — chân nào ngắt được, giới hạn khi ngủ, wake source | `pin-constraints.md` |
| 8 | Boot/debug/USB conflict — UART0 console, USB-Serial-JTAG, JTAG pin | `boot-debug-usb.md` |
| 9 | Flash/PSRAM restriction — chân bị SPI flash và PSRAM octal chiếm | `flash-psram.md` |
| 10 | Power risk — dòng đỉnh, brownout, inrush, tải cảm ứng, trạng thái boot | `power-electrical.md` |

Chỉ đọc reference của hạng mục thực sự đang xét. Không nạp cả thư mục.

## Mẫu báo cáo (bám sát, không viết dài)

```
## Cấu hình
SoC: <chính xác tên module> | Board: <tên hoặc "custom"> | Nguồn bằng chứng: <hạng 1-3>

## Pin map
| Ngoại vi | Chân | Hướng | Pull | Ghi chú | Trạng thái |
|---|---|---|---|---|---|
| I2C0 SDA | GPIO8 | OD | ngoài 4.7k | | OK |
| Relay    | GPIO4 | OUT | xuống 10k | strapping C6 | CẢNH BÁO |

## Phát hiện
CHẶN   [#2] GPIO12 dùng cho LED — kéo cao lúc boot đặt flash về 1.8V, module không khởi động.
            Nguồn: TRM ESP32 §Strapping Pins. Sửa: chuyển sang GPIO13 hoặc thêm pull-down 10k.
CẢNH BÁO [#6] I2C chưa rõ có pull-up ngoài chưa; pull nội quá yếu cho 400kHz.
THÔNG TIN [#3] Còn dư 1 SPI host, 2 kênh LEDC.

## Thiếu thông tin — cần bạn xác nhận
1. Board có pull-up trên SDA/SCL không? (cần schematic hoặc đo điện trở giữa SDA và 3V3)
2. Cảm biến X dùng mức logic 3.3V hay 5V? (cần datasheet trang Electrical Characteristics)

## Kết luận
<Chốt được / Chốt được sau khi sửa N mục CHẶN / Chưa chốt được, còn N câu hỏi>
```

Mức độ: **CHẶN** (không được nạp firmware trước khi sửa) / **CẢNH BÁO** (chạy được nhưng có rủi ro)
/ **THÔNG TIN**.

## Không thuộc scope
- Viết code cấu hình ngoại vi → `esp32-04-driver-development`
- Chia task, quyết định ISR chạy ở đâu → `esp32-03-firmware-architecture`
- Đo dòng thực tế, tối ưu sleep → `esp32-11-power-management`
- Cấu hình flash size / partition trong sdkconfig → `esp32-01-project-init`

## References

- `checklists/analysis-checklist.md` — 10 hạng mục kiểm tra, mẫu câu hỏi khi thiếu thông tin
- `references/chip-matrix.md` — năng lực theo dòng SoC; cái gì phải tra datasheet
- `references/pin-constraints.md` — chân cấm, input-only, pull, interrupt, RTC/wake
- `references/peripheral-matrix.md` — số instance, xung đột timer/DMA/host, IOMUX vs GPIO matrix
- `references/boot-debug-usb.md` — strapping, download mode, UART0, USB, JTAG, điện áp flash
- `references/flash-psram.md` — chân bị flash/PSRAM chiếm, hậu tố module, cấu hình flash
- `references/power-electrical.md` — mức logic, dòng đỉnh, brownout, tải ngoài, an toàn lúc boot
- `templates/pin-map.md` — mẫu tài liệu pin map kèm bảng rủi ro và câu hỏi treo

## Đầu ra
Pin map đã thẩm định (ghi vào `board_config.h`) + danh sách rủi ro + danh sách câu hỏi còn treo.
