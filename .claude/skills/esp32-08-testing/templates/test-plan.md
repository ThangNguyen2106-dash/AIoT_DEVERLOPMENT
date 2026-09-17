# Test plan — <tên dự án / feature>

Ngày: <yyyy-mm-dd> | Firmware: <version/commit> | SoC: <module> | Mức dự án: <simple/medium/production>

## Mức kiểm chứng hiện tại

`BUILD` / `HOST` / `QEMU` / `TARGET` / `HARDWARE` / `SOAK` — **<chọn một>**

## Tầng 1 — Build / static validation

| Kiểm tra | Trạng thái | Ghi chú |
|---|---|---|
| Build đa target (<liệt kê>) | | |
| Không warning mới | | |
| Static analysis | | |
| Kích thước app / slot OTA | | __% |
| Partition + sdkconfig hợp lệ | | |
| QEMU smoke | | |

## Tầng 2 — Software testing

### Host test
| Module | Case | Phủ gì | Kết quả |
|---|---|---|---|
| | | biên / đầu vào hỏng / bảng trạng thái | |

### Unit test trên target
| Test | Tag | Cần phần cứng gì | Kết quả |
|---|---|---|---|
| | `[...]` | loopback TX–RX GPIO?–GPIO? | |

### Integration test
| Đường dữ liệu | Kiểm gì | Kết quả |
|---|---|---|
| | | |

### Regression test
| Bug / issue | Ngày | Test chặn | Đã fail trên code cũ? |
|---|---|---|---|
| | | | |

### Fault injection
| Kịch bản | Mong đợi | Thực tế | Kết luận |
|---|---|---|---|
| | | | đạt / không đạt / đã sửa |

## Tầng 3 — Hardware validation

### Peripheral
| Ngoại vi | Kiểm gì | Thiết bị đo | Số đo | Đạt? |
|---|---|---|---|---|
| | | | | |

### Communication
| Kịch bản | Số đo | Đạt? |
|---|---|---|
| Kết nối lần đầu, NVS rỗng | __ s | |
| Rớt AP __ phút → phục hồi | __ s | |
| Dữ liệu offline gửi bù | mất __ / trùng __ | |

### Điện & timing
| Hạng mục | Số đo | Đạt? |
|---|---|---|
| Dòng đỉnh | __ mA | |
| Trạng thái chân lúc boot | | |
| Độ trễ đường quan trọng | __ ms | |

### Soak
| Hạng mục | Bắt đầu | Kết thúc | Xu hướng |
|---|---|---|---|
| Thời lượng | | | __ giờ |
| Min free heap | | | |
| Khối liên tục lớn nhất | | | |
| Stack HWM thấp nhất | | | |
| Reset ngoài kế hoạch | | | __ lần |

## Chưa chạy được — cần thực hiện trên phần cứng

| Việc | Cách chạy | Kết quả mong đợi | Ai làm |
|---|---|---|---|
| | | | |

## Definition of Done

| # | Mục | Kết quả | Ghi chú |
|---|---|---|---|
| 1 | Build sạch | | |
| 2 | Host test logic thuần | | |
| 3 | Chạy trên phần cứng thật | | |
| 4 | Đường lỗi đã thử | | |
| 5 | Regression test | | |
| 6 | Reset / mất nguồn / mất mạng | | |
| 7 | Tài nguyên ổn định | | |
| 8 | Quan sát được từ xa | | |
| 9 | Cấu hình & tài liệu đi kèm | | |

**Kết luận:** <Xong / Chưa xong — còn N mục: ...>
