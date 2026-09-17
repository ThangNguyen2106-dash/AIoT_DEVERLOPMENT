# Kiến trúc firmware — <tên dự án>

Ngày: <yyyy-mm-dd> | SoC: <module> | Framework: <ESP-IDF x.y / PlatformIO / Arduino>

## 1. Mức dự án

**<simple / medium / production>**

| Tiêu chí | Trả lời |
|---|---|
| Số thiết bị | |
| Có OTA | |
| Tiếp cận được để sửa | |
| Tuổi thọ | |
| Số người maintain | |
| Hậu quả khi hỏng | |

## 2. Phân lớp

```
app/      <tên> — <trách nhiệm một dòng>
service/  <tên> — ...
driver/   <tên> — ...
hal/      <có/không — lý do>
```

Hướng phụ thuộc: `app → service → driver → hal`. Cấm ngược, cấm vòng.

## 3. Sơ đồ task

| Task | Prio | Stack | Core | Block trên | Nhịp | Sở hữu tài nguyên |
|---|---|---|---|---|---|---|
| | | | | | | |

Thứ tự khởi tạo: <liệt kê, tường minh>

## 4. Giao tiếp

| Từ → Đến | Cơ chế | Kích thước/depth | Khi đầy | Lý do chọn |
|---|---|---|---|---|
| | | | | |

## 5. State machine

| State | Vào khi | Ra khi | Timeout | Đi tới |
|---|---|---|---|---|
| | | | | |

## 6. Cấu hình

| Giá trị | Loại | Nơi chứa | Mặc định |
|---|---|---|---|
| | | | |

## 7. Storage

| Dữ liệu | Nơi chứa | Sống qua OTA | Schema ver |
|---|---|---|---|
| | | | |

## 8. Chính sách lỗi

| Lỗi | Phân loại | Xử lý |
|---|---|---|
| | fatal / retry / degrade | |

Trạng thái an toàn của cơ cấu chấp hành khi lỗi: <mô tả>

## 9. Kết luận 13 hạng mục

| # | Hạng mục | Kết luận | Ghi chú / điều kiện kích hoạt |
|---|---|---|---|
| 1 | Application layer | | |
| 2 | Service layer | | |
| 3 | Driver layer | | |
| 4 | Hardware abstraction | | |
| 5 | Configuration | | |
| 6 | Communication | | |
| 7 | Event system | | |
| 8 | Task architecture | | |
| 9 | State machine | | |
| 10 | Error handling | | |
| 11 | Logging | | |
| 12 | Storage | | |
| 13 | OTA | | |

## 10. Rủi ro kiến trúc

| Vấn đề | Hệ quả | Cách chặn |
|---|---|---|
| | | |

## 11. Câu hỏi còn treo

1.
2.
