# Kích thước binary và slot OTA

## Ràng buộc thật là slot OTA, không phải flash

Có OTA thì app phải vừa **một slot**, không phải cả flash. Flash 4 MB với hai slot OTA
thường cho mỗi slot ~1.5-1.9 MB. Vượt slot → không cập nhật được nữa, kể cả khi flash còn trống.

```bash
idf.py size                 # tổng quan: DRAM / IRAM / Flash code / Flash data
idf.py size-components      # component nào chiếm bao nhiêu  ← bắt đầu từ đây
idf.py size-files           # chi tiết tới từng file
```

Đối chiếu với `partitions.csv` (`esp32-01-project-init/references/partitions.md`).
Giữ **biên dự phòng ≥ 15%** cho slot OTA: firmware chỉ có lớn lên, và một bản vá khẩn cấp
không vừa slot là tình huống rất xấu.

## Giảm theo thứ tự hiệu quả

| # | Biện pháp | Thu được điển hình | Mất gì |
|---|---|---|---|
| 1 | Hạ mức log mặc định VERBOSE/DEBUG → INFO (`CONFIG_LOG_DEFAULT_LEVEL`) | 50-150 KB | chi tiết khi gỡ lỗi tại chỗ |
| 2 | Bỏ component không dùng khỏi `REQUIRES`/`idf_component.yml` | thay đổi lớn | — |
| 3 | `CONFIG_COMPILER_OPTIMIZATION_SIZE` (`-Os`) | 5-15% | khó debug hơn một chút |
| 4 | Tắt `CONFIG_ESP_ERR_TO_NAME_LOOKUP` | ~10-20 KB | `esp_err_to_name()` chỉ còn mã số |
| 5 | Dùng CA cụ thể thay vì cả `esp_crt_bundle` | ~60 KB | phải tự quản lý CA khi hết hạn |
| 6 | Bỏ `assert` ở production (`CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_DISABLE`) | vài chục KB | **mất lưới an toàn — cân nhắc kỹ** |
| 7 | Tắt console/monitor ở production | vài KB | mất chẩn đoán qua UART |

Biện pháp 5 và 6 chạm bảo mật và khả năng chẩn đoán → hỏi `esp32-10-security` trước khi áp dụng,
đừng tự quyết.

## Cẩn thận: log không chỉ tốn flash

Chuỗi format của `ESP_LOGx` nằm trong flash, nhưng bản thân việc log:
- tốn thời gian UART (115200 baud ≈ 11 µs/ký tự — một dòng 80 ký tự ≈ 0.9 ms),
- có thể làm lệch timing của driver và gây jitter (`timing.md`),
- có thể làm tràn buffer và mất log quan trọng.

Giảm mức log thường cải thiện **cả** kích thước **và** thời gian thực.

## IRAM tràn

`section '.iram0.text' will not fit` là lỗi **link**, không phải hết heap. Nguyên nhân:
quá nhiều hàm gắn `IRAM_ATTR`, hoặc bật `CONFIG_SPI_FLASH_ROM_IMPL`/các tuỳ chọn kéo code vào IRAM.

Cách xử lý: xem `timing.md` mục IRAM — chỉ hàm **thật sự** chạy khi cache tắt mới cần `IRAM_ATTR`.
Gắn bừa "cho nhanh" là nguyên nhân phổ biến nhất.

## Theo dõi theo thời gian

Đưa kích thước app vào CI (`esp32-08-testing/references/static-validation.md`):
in `idf.py size` mỗi lần build, cảnh báo khi vượt ngưỡng slot. Phát hiện lúc commit rẻ hơn
phát hiện lúc chuẩn bị release rất nhiều.
