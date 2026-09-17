# Cổng gỡ lỗi và rò rỉ thông tin

Cổng gỡ lỗi là đường vào rẻ nhất cho người cầm được thiết bị: JTAG đọc thẳng RAM (và bí mật
đang nằm trong RAM), UART download mode đọc cả flash, console log kể hết nội tình.

## Bảng quyết định

| Cổng | Prototype | Production | Cách tắt |
|---|---|---|---|
| Console log UART | mở, VERBOSE | INFO trở xuống, cân nhắc tắt hẳn | `CONFIG_BOOTLOADER_LOG_LEVEL`, `CONFIG_LOG_DEFAULT_LEVEL` |
| Log tag nhạy cảm | mở | không log bí mật ở mọi mức | rà bằng grep |
| JTAG | mở | vô hiệu hoá (eFuse) | `espefuse.py burn_efuse DIS_USB_JTAG` / `JTAG_DISABLE` — **user tự chạy** |
| UART download mode | mở | vô hiệu hoá ở bước cuối | `espefuse.py burn_efuse DIS_DOWNLOAD_MODE` — **user tự chạy** |
| gdbstub | dùng khi cần | tắt | `CONFIG_ESP_SYSTEM_PANIC_GDBSTUB=n` |
| Core dump UART | bật | cân nhắc | `CONFIG_ESP_COREDUMP_ENABLE_TO_UART` |
| Core dump flash | bật | bật (tiện chẩn đoán) | `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH` |
| Web/telnet/debug shell | tuỳ | tắt hoặc có xác thực | bỏ khỏi build production |

Trợ lý **không tự chạy** lệnh `espefuse.py` nào. Đốt eFuse là một chiều.

## Thứ tự ở bước sản xuất

Đốt các eFuse vô hiệu hoá gỡ lỗi là **thao tác cuối cùng**, sau khi:
đã nạp firmware, nạp NVS bí mật, chạy test chức năng, xác nhận OTA hoạt động.
Đốt sớm thì mọi lỗi phát hiện sau đó đều phải vứt board.

`espefuse.py summary` sau khi đốt là bước QA bắt buộc — thiết bị chưa đốt đủ thì không xuất xưởng.

## Log rò gì

Kiểm cụ thể, đừng chỉ hạ mức log:

- Bí mật in ra khi debug rồi quên bỏ (`ESP_LOGD(TAG, "pass=%s", pass)`) — mức DEBUG vẫn ra UART
  nếu ai đó bật lại, và vẫn nằm trong binary dưới dạng chuỗi.
- `strings firmware.bin | grep -i "password\|key\|token"` — chạy thử trên ảnh sắp ship.
- Log kể chi tiết cấu trúc nội bộ, URL backend, tên topic MQTT: giúp kẻ tấn công dựng bản đồ hệ thống.
  Production nên log mã lỗi ngắn, không log câu văn chi tiết.
- Panic/backtrace in ra UART tiết lộ layout bộ nhớ. Chấp nhận được với hầu hết sản phẩm; sản phẩm
  có yêu cầu bảo mật cao thì chuyển sang core dump vào flash, đọc qua kênh đã xác thực.

## Cổng vật lý khác cũng phải tính

- **USB Serial/JTAG** (C3, S3, C6): tiện gỡ lỗi, cũng là JTAG. Có eFuse riêng để vô hiệu.
- **Chân boot (GPIO0)**: kéo xuống lúc reset là vào download mode. Không dùng phần mềm chặn được
  — phải dùng eFuse `DIS_DOWNLOAD_MODE`.
- **Chip flash rời**: hàn ra đọc được toàn bộ nếu không có Flash Encryption.
- **Test point trên PCB**: sản phẩm thật nên bỏ hoặc che test point UART/JTAG — vấn đề phần cứng,
  nhắc user khi rà soát.

## Chẩn đoán từ xa thay cho cổng vật lý

Đóng hết cổng thì phải mở đường chẩn đoán khác, nếu không mọi lỗi ngoài thực địa đều mù:

- Core dump lưu vào partition, gửi lên server khi có mạng (`esp32-12-release`).
- Thống kê tự báo: reset reason, uptime, heap thấp nhất, số lỗi giao tiếp theo driver
  (`*_get_stats()` ở `esp32-04-driver-development`).
- Chế độ chẩn đoán mở tạm bằng lệnh **có xác thực** từ cloud, tự tắt sau N phút — không phải
  một cổng mở thường trực.
