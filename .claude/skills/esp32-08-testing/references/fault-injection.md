# Fault injection

Firmware ngoài thực địa hỏng ở **đường lỗi**, không ở đường thành công — vì đường thành công
là thứ duy nhất được thử trên bàn. Fault injection là cố tình tạo ra điều kiện xấu để
xác nhận firmware xử lý đúng.

## Khi nào làm

| Làm | Không làm |
|---|---|
| Mức medium/production | Prototype vứt đi sau vài tuần |
| Thiết bị không tiếp cận được để reset tay | Thiết bị luôn có người bên cạnh |
| Có cơ cấu chấp hành (relay, van, motor, sưởi) | Chỉ đọc và hiển thị |
| Có mạng, có nguồn không ổn định | Cắm nguồn cố định trong phòng |

Chi phí không nhỏ. Chọn 3–5 kịch bản đau nhất, không làm cho đủ.

## Kịch bản theo nhóm

### Ngoại vi
| Tiêm gì | Cách làm | Mong đợi |
|---|---|---|
| Cảm biến không phản hồi | rút dây SDA hoặc cấp nguồn cảm biến khi đang chạy | timeout có giới hạn, log WARN, retry, không treo |
| Dữ liệu hỏng | ép CRC sai bằng test hook | loại bỏ mẫu, không dùng giá trị rác để điều khiển |
| Bus kẹt mức thấp | kéo SDA xuống GND | phát hiện, thử recovery clock, báo lỗi, không khoá task |
| Ngoại vi cắm lại | rút rồi cắm | tự re-init và phục hồi, không cần reset |

### Nguồn
| Tiêm gì | Cách làm | Mong đợi |
|---|---|---|
| Mất nguồn giữa lúc ghi NVS | cắt nguồn lặp lại nhiều lần khi đang ghi | boot lại đọc được cấu hình cũ hoặc mới, không phải rác |
| Sụt áp | nguồn lập trình được hạ xuống ngưỡng brownout | brownout reset sạch, cơ cấu chấp hành về trạng thái an toàn |
| Mất nguồn giữa OTA | cắt khi đang tải | boot lại bằng slot cũ, thiết bị không brick |

### Mạng
| Tiêm gì | Cách làm | Mong đợi |
|---|---|---|
| Rớt Wi-Fi | tắt AP, hoặc `esp_wifi_disconnect()` | reconnect có backoff, không spam, không tràn log |
| Server không phản hồi | firewall drop (không reject) | timeout đúng hạn, không treo task |
| Đứt giữa chừng | cắt kết nối khi đang gửi | dữ liệu vào buffer, gửi lại khi có mạng, không mất/không trùng |
| Phản hồi rác | server giả trả JSON sai/gói quá dài | từ chối an toàn, không tràn buffer (`esp32-10-security`) |
| DNS hỏng, giờ sai | đổi DNS/NTP | TLS lỗi được báo rõ, không im lặng chấp nhận |

### Tài nguyên
| Tiêm gì | Cách làm | Mong đợi |
|---|---|---|
| Hết heap | cấp phát chiếm chỗ trong test hook | `malloc` trả NULL được xử lý, không deref NULL |
| Queue đầy | ngừng consumer | hành vi đúng như đã thiết kế (bỏ cũ/bỏ mới/chặn), không mất im lặng |
| Task bị đói CPU | tạo task priority cao busy một lúc | watchdog báo đúng task, không hỏng dữ liệu |
| Flash gần đầy | ghi file tới gần hết | báo lỗi rõ, không ghi đè lung tung |

## Cách tiêm lỗi trong code

Dùng hook chỉ tồn tại trong build test, **không để đường tiêm lỗi tồn tại trong
firmware production**:

```c
#if CONFIG_APP_FAULT_INJECTION
esp_err_t fault_maybe_fail(const char *point);   /* trả lỗi theo cấu hình test */
#define FAULT_POINT(p) do { esp_err_t _e = fault_maybe_fail(p); if (_e) return _e; } while (0)
#else
#define FAULT_POINT(p) do { } while (0)
#endif
```

Đặt `FAULT_POINT()` ở đúng các ranh giới I/O đã thiết kế, không rải khắp nơi.
Kconfig mặc định tắt; CI bật trong một job riêng.

## Ghi lại kết quả

Mỗi kịch bản ghi 4 thứ: tiêm gì — mong đợi gì — thực tế gì — kết luận (đạt / không đạt / đã sửa).
Kịch bản không đạt mà chưa sửa phải nằm trong danh sách rủi ro khi release
(`esp32-12-release`), không được im lặng bỏ qua.

## Ranh giới

- Gỡ một lỗi đã xảy ra → `esp32-07-debugging`
- Xác thực dữ liệu đầu vào độc hại → `esp32-10-security`
- Chính sách reconnect/backoff → `esp32-05-connectivity`
