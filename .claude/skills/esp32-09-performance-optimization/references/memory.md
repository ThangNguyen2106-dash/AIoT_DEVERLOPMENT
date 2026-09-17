# RAM: heap, stack, phân mảnh, PSRAM

Trước khi đọc file này, xác nhận đã loại trừ **rò bộ nhớ** (`measure.md`, bảng cuối).
Rò là bug → `esp32-07-debugging`. File này nói về firmware đúng nhưng không vừa RAM.

## Bản đồ RAM ESP32 — biết mình đang hết loại nào

| Loại | Dùng cho | Hết thì |
|---|---|---|
| DRAM (internal) | heap thường, biến `.bss`/`.data`, stack task | `malloc` trả NULL |
| IRAM | code chạy khi cache bị tắt, ISR có `IRAM_ATTR` | link lỗi "section overflow" |
| DMA-capable | buffer cho SPI/I2S/Wi-Fi DMA | `heap_caps_malloc(MALLOC_CAP_DMA)` trả NULL |
| RTC slow/fast | `RTC_DATA_ATTR`, giữ qua deep sleep | → `esp32-11-power-management` |
| PSRAM (nếu có) | buffer lớn, không phải mọi thứ | xem mục PSRAM dưới |

`heap_caps_print_heap_info(MALLOC_CAP_INTERNAL)` và `MALLOC_CAP_DMA` cho biết đang hết loại nào.
"Hết RAM" chung chung không đủ để chọn cách sửa.

## Heap: giảm chỗ dùng nhiều nhất trước

Thủ phạm thường gặp, theo thứ tự:

| Nguồn | Lượng điển hình | Cách giảm |
|---|---|---|
| mbedTLS / TLS handshake | 16-40 KB mỗi kết nối lúc bắt tay | giảm `CONFIG_MBEDTLS_SSL_IN/OUT_CONTENT_LEN` (4 KB nếu server chấp nhận); dùng lại session; không mở nhiều TLS đồng thời |
| Wi-Fi buffer | 20-40 KB | giảm số `static`/`dynamic rx/tx buffer` trong menuconfig — **đo lại throughput sau khi giảm** |
| Buffer MQTT / HTTP | theo cấu hình | đặt đúng kích thước message thật, không để mặc định |
| JSON parser | gấp 1.5-3× kích thước payload | parse theo stream, hoặc dùng định dạng nhị phân |
| Stack các task | 3-8 KB mỗi task | xem mục Stack |
| Log buffer, console | vài KB | giảm mức log, tắt console ở production |

Quy tắc: **cấp phát một lần lúc init, giữ suốt đời chương trình.** Buffer cấp/giải phóng lặp lại
trong vòng lặp vừa tốn thời gian vừa gây phân mảnh.

## Stack

- Đặt stack bằng **đo**, không bằng cảm tính: chạy nhánh sâu nhất dưới tải thực, đọc
  `uxTaskGetStackHighWaterMark`, giữ dư **≥ 25%** (con số chuẩn toàn bộ skill).
- Ngốn stack nhiều nhất: mbedTLS (8-16 KB), `printf`/`snprintf` họ `%f`, JSON parser đệ quy,
  và **buffer khai báo cục bộ** (`char buf[4096];` trong hàm).
- Buffer lớn không bao giờ để trên stack. Đưa lên heap (cấp một lần) hoặc làm `static`.
- Giảm stack để tiết kiệm RAM mà không đo là đổi crash lấy vài KB — bị cấm ở SKILL.md.
- Task tạo bằng `xTaskCreateStatic` cho phép đặt stack vào vùng chỉ định và biết trước chi phí.

## Phân mảnh

Dấu hiệu: `free heap` còn nhiều nhưng `heap_caps_get_largest_free_block()` nhỏ, và cấp phát
khối lớn thất bại dù tổng còn dư.

Nguyên nhân gần như luôn là: cấp phát và giải phóng các khối **kích thước khác nhau** lặp đi
lặp lại trong thời gian dài.

Cách chữa, theo thứ tự:
1. Chuyển sang cấp phát một lần lúc init, dùng lại buffer đó.
2. Dùng pool cố định: N khối cùng kích thước, cấp phát bằng cách lấy khối rỗi trong pool.
3. Tách buffer lớn ra vùng riêng (PSRAM) để không băm nhỏ heap nội.
4. Nếu vẫn phải cấp phát động: cấp phát theo **vài kích thước chuẩn** thay vì kích thước tuỳ ý.

Không có cách "gom mảnh" ở runtime — heap của ESP-IDF không nén được. Phòng là cách duy nhất.

## PSRAM — không phải viên đạn bạc

Chỉ có trên module có hậu tố PSRAM (ví dụ `...-N16R8` = 16 MB flash + 8 MB PSRAM).
Kiểm năng lực chip trước ở `esp32-02-hardware-analysis/references/flash-psram.md`.

| Sự thật | Hệ quả |
|---|---|
| PSRAM chậm hơn DRAM nội đáng kể (qua cache SPI) | không đặt cấu trúc dữ liệu truy cập nóng vào đây |
| **Không phải ngoại vi nào cũng DMA được từ PSRAM** | buffer DMA phải `heap_caps_malloc(n, MALLOC_CAP_DMA \| MALLOC_CAP_INTERNAL)` — kiểm TRM trước |
| Truy cập PSRAM bị chặn khi cache tắt (ghi flash, một số ISR) | dữ liệu dùng trong ISR không để ở PSRAM |
| `CONFIG_SPIRAM_USE_MALLOC` cho `malloc` tự dùng PSRAM | tiện nhưng làm mờ ranh giới — ưu tiên `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` tường minh |

Dùng PSRAM cho: framebuffer màn hình, buffer audio/ảnh, hàng đợi dữ liệu lớn.
Không dùng cho: biến trạng thái nóng, buffer DMA (trừ khi đã xác nhận ngoại vi hỗ trợ), dữ liệu ISR.

## Khi thật sự không đủ RAM

Theo thứ tự, dừng ở bước đầu tiên đủ dùng:

1. Giảm buffer TLS/Wi-Fi/MQTT về đúng nhu cầu thật (thường thu được nhiều nhất).
2. Bỏ component không dùng — mỗi component kéo theo `.bss` của nó.
3. Gộp task: 2 task nhỏ chờ cùng một nguồn sự kiện thường nên là một task (bàn ở `esp32-03`).
4. Xử lý theo stream thay vì nạp cả message vào RAM.
5. Đổi sang module có PSRAM — quyết định phần cứng, chuyển `esp32-02-hardware-analysis`.
