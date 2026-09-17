# Throughput: DMA, kích thước giao dịch, bus, mạng

## Luật chung: chi phí nằm ở số lần, không ở số byte

Với mọi bus, mỗi **giao dịch** có chi phí cố định (setup, ngắt, chuyển ngữ cảnh) lớn hơn nhiều
chi phí truyền thêm một byte. Đọc 1024 byte một lần nhanh hơn đọc 1 byte 1024 lần **hàng chục lần**,
dù cùng số byte.

Việc đầu tiên khi throughput thấp: **đếm số giao dịch mỗi giây**, không phải đếm byte.

## DMA

DMA giải phóng CPU khỏi việc copy, và thường là cách duy nhất đạt tốc độ bus cao.

| Ràng buộc | Chi tiết |
|---|---|
| Buffer phải DMA-capable | `heap_caps_malloc(n, MALLOC_CAP_DMA)` — không phải `malloc` thường |
| Căn chỉnh | thường 4 byte; một số ngoại vi (I2S, LCD) yêu cầu chặt hơn — tra TRM |
| **Không phải ngoại vi nào cũng DMA được từ PSRAM** | kiểm trước khi chuyển buffer sang PSRAM (`memory.md`) |
| Buffer không được nằm trên stack | stack có thể không DMA-capable, và bị ghi đè khi hàm thoát |
| Kích thước giao dịch tối đa | có giới hạn theo ngoại vi; chia gói nếu vượt |

Mẫu đúng: cấp phát buffer DMA **một lần lúc init**, dùng lại; dùng hàng đợi giao dịch để
CPU chuẩn bị gói kế tiếp trong lúc DMA đang chuyển gói hiện tại.

## SPI

- Dùng `spi_device_queue_trans()` + `spi_device_get_trans_result()` thay vì
  `spi_device_transmit()` (blocking) khi cần tốc độ — cho phép chồng lấn CPU và truyền.
- Giao dịch ≤ 4 byte dùng `SPI_TRANS_USE_TXDATA`/`RXDATA` để tránh cấp phát.
- Tốc độ thật bị giới hạn bởi dây và thiết bị, không chỉ bởi cấu hình. Vượt quá → lỗi dữ liệu
  im lặng, không phải lỗi trả về. Tăng tốc độ phải kèm kiểm tra tính đúng (CRC).
- Chân đi qua GPIO matrix chậm hơn chân IOMUX ở tần số cao → `esp32-02/references/peripheral-matrix.md`.

## I2C

I2C là bus chậm theo thiết kế (100/400 kHz, một số thiết bị 1 MHz). Không tối ưu được nhiều:
- Gộp nhiều thanh ghi liên tiếp vào **một** lần đọc burst thay vì đọc từng thanh ghi.
- Không đọc lại giá trị cấu hình mỗi chu kỳ — đọc một lần, nhớ trong driver.
- Chu kỳ đọc cảm biến thường bị giới hạn bởi **thời gian chuyển đổi của cảm biến**, không bởi bus.
  Tách `start()` / `read()` thay vì chờ (quy tắc ở `esp32-04-driver-development`).
- Hạ tốc độ bus khi chưa đo là hành vi bị cấm ở `esp32-07-debugging`.

## UART

- Bật FIFO và dùng event queue (`uart_driver_install` với queue) thay vì polling.
- Đọc theo **frame** với timeout, không đọc từng byte.
- Ở tốc độ cao (≥ 921600) mà mất byte: kiểm ngưỡng FIFO, kích thước buffer driver, và
  xem task đọc có bị task khác chiếm CPU không (`timing.md`).

## Mạng (Wi-Fi / Ethernet)

| Vấn đề | Nguyên nhân hay gặp |
|---|---|
| Throughput thấp hơn nhiều so với lý thuyết | buffer Wi-Fi bị cắt quá tay khi tối ưu RAM; TCP window nhỏ |
| CPU cao khi truyền | copy nhiều lần giữa các lớp; gói quá nhỏ |
| Trễ nhưng băng thông ổn | Nagle + delayed ACK; cân nhắc `TCP_NODELAY` cho gói nhỏ có yêu cầu độ trễ |
| Tốc độ tụt khi bật TLS | chi phí mã hoá là thật; đo riêng phần TLS trước khi đổ lỗi cho mạng |

Gửi nhiều gói nhỏ tốn hơn gửi ít gói lớn rất nhiều — **gộp bản ghi** trước khi gửi.
Nhưng gộp làm tăng độ trễ dữ liệu và tăng rủi ro mất cả lô khi reset: đánh đổi này thuộc
`esp32-05-connectivity` (buffering) và `esp32-11-power-management` (nếu chạy pin).
Đừng tự quyết ở skill này.

## Cân đo trước khi tối ưu

Với mỗi đường dữ liệu, tính **giới hạn lý thuyết** trước:

```
I2C 400 kHz, đọc 6 byte + overhead ≈ 10 byte × 9 bit ≈ 225 µs/giao dịch
→ trần lý thuyết ≈ 4400 giao dịch/s
```

Đang đạt 80% trần lý thuyết thì không còn gì để tối ưu ở phần mềm — phải đổi bus, đổi tốc độ,
hoặc đổi kiến trúc dữ liệu. Biết điều này sớm tiết kiệm được nhiều ngày.
