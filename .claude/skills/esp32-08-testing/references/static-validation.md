# Tầng 1 — Build / static validation

Tự động hoàn toàn, chạy mỗi lần push. Đây là lưới chặn rẻ nhất. Không có tầng này thì
mọi lỗi tầm thường đều phải trả giá bằng thời gian nạp board.

## 1. Build đa target

Build cho **mọi chip target dự án hỗ trợ**, không chỉ chip đang cắm trên bàn.

```bash
for t in esp32 esp32s3 esp32c3 esp32c6; do
  idf.py set-target "$t" && idf.py build || exit 1
done
```

Bắt được: dùng API không tồn tại trên chip đó (DAC trên C3, touch trên C6), dùng chân
không có, giả định số instance ngoại vi. Đây là lớp lỗi hay lọt nhất và rẻ nhất để chặn.

## 2. Warning coi như lỗi

- Bật `-Wall -Wextra`; trong CI thêm `-Werror`.
- Warning đáng sợ nhất trong firmware: `-Wformat` (sai kiểu trong `ESP_LOGx` → crash),
  `-Wreturn-type`, `-Wuninitialized`, `-Wswitch` (thiếu case trong enum state machine),
  `-Wimplicit-fallthrough`.
- Không tắt warning bằng cách sửa flag. Sửa code, hoặc chặn cục bộ bằng pragma kèm
  comment lý do.

## 3. Static analysis

| Công cụ | Bắt cái gì |
|---|---|
| `cppcheck` | con trỏ NULL, buffer overrun, biến chưa khởi tạo, code chết |
| `clang-tidy` | quy ước, chuyển kiểu nguy hiểm, logic khả nghi |
| `idf.py clang-check` | tích hợp sẵn với ESP-IDF |

Đưa vào CI ở chế độ cảnh báo trước, siết dần. Đừng bật hết mọi rule ngay — nhiễu
nhiều thì không ai đọc.

## 4. Kiểm tra kích thước binary

```bash
idf.py size          # tổng quan
idf.py size-components   # ai chiếm chỗ
```

- So kích thước app với **slot OTA**, không so với tổng flash. Vượt slot = không OTA được
  nữa, phát hiện muộn thì rất đau.
- Theo dõi kích thước qua từng commit; cảnh báo trong CI khi tăng đột biến hoặc vượt
  ngưỡng (ví dụ 85% slot).
- Kiểm cả DRAM/IRAM còn lại, không chỉ flash.

## 5. Kiểm tra cấu hình

- `sdkconfig.defaults` có nằm trong git không? (`sdkconfig` thì không nên)
- Partition table: tổng có vừa flash thật không? Có `otadata` + hai slot OTA nếu dự án
  có OTA không?
- Log level production đã đặt đúng chưa? Assert có bị tắt ngoài ý muốn không?
- Không có bí mật nào bị commit (`grep` token/password/khoá trong source).

## 6. Smoke test trên QEMU

```bash
idf.py qemu
```

Chạy được với firmware không phụ thuộc ngoại vi thật. Chứng minh: boot qua bootloader,
init không abort, task khởi tạo được, không panic trong vài giây đầu. Đây là mức `QEMU` —
mạnh hơn `BUILD`, yếu hơn `TARGET`.

## Cổng CI tối thiểu

| Kiểm tra | Chặn merge? |
|---|---|
| Build mọi target | có |
| Không warning mới | có |
| Host test pass | có |
| Kích thước ≤ ngưỡng slot OTA | có |
| Static analysis | cảnh báo (siết dần) |
| QEMU smoke | có nếu firmware chạy được không cần ngoại vi |

Chi tiết cấu hình CI → `unity-and-ci.md`.
