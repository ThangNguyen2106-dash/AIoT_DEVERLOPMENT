# Ghim phiên bản và quản lý dependency

## Luật: không có gì được trôi

Firmware build lại sau 8 tháng phải cho **đúng binary cũ**. Không ghim phiên bản thì:
một bản vá khẩn cấp build ra một firmware khác với bản đang chạy ngoài thực địa, và bạn
không biết khác chỗ nào.

Bốn thứ phải ghim:

| Thứ | Ghim bằng |
|---|---|
| ESP-IDF | tag git cụ thể (`v5.1.2`), **không** phải nhánh `release/v5.1` |
| Component từ registry | `idf_component.yml` với phiên bản chính xác |
| Thư viện nhúng trực tiếp | git submodule ở commit cụ thể, hoặc chép vào repo |
| Toolchain | đi kèm phiên bản IDF — đừng trộn toolchain của bản khác |

## ESP-IDF

```bash
cd $IDF_PATH
git checkout v5.1.2            # tag, không phải nhánh
git submodule update --init --recursive
./install.sh esp32s3
```

Ghi phiên bản vào `README.md` của dự án và vào CI. Nâng cấp IDF là một **việc riêng có kế hoạch**,
không làm kèm trong lúc thêm tính năng: nó đổi API (`esp32-firmware/references/idf-versions.md`),
đổi kích thước binary, và đổi hành vi ở những chỗ không ngờ.

Sau mỗi lần nâng cấp IDF: build đủ mọi target, đo lại kích thước binary, chạy lại toàn bộ test,
và chạy soak test. Coi như một release (`esp32-12-release`).

PlatformIO tương đương:
```ini
[env:esp32s3]
platform = espressif32@6.5.0    ; ghim đúng phiên bản, không dùng ^ hay ~
framework = espidf
board = esp32-s3-devkitc-1
```

## Component từ ESP Component Registry

`main/idf_component.yml`:

```yaml
dependencies:
  idf:
    version: ">=5.1"
  espressif/mdns:
    version: "1.2.0"          # chính xác, không "^1.2.0"
  joltwallet/littlefs:
    version: "1.14.8"
```

- `managed_components/` **không vào git** — nó được tải lại từ `idf_component.yml`.
- `dependencies.lock` **nên vào git** — đây là thứ thực sự đảm bảo tái tạo được.
- Component từ registry là code của người khác chạy trên thiết bị của bạn. Trước khi thêm:
  nó có được bảo trì không, có bao nhiêu người dùng, license có phù hợp không.

## Đánh giá trước khi thêm một thư viện

Mỗi dependency là mã bạn phải chịu trách nhiệm nhưng không kiểm soát. Ba câu hỏi:

1. **Nó tiết kiệm bao nhiêu?** Wrapper 200 dòng quanh một cảm biến I2C thường tự viết nhanh hơn
   là đọc hiểu, và viết ra thì đúng chuẩn của bộ skill này (timeout, mã lỗi, state).
2. **Nó tốn bao nhiêu?** Flash, RAM, và những phụ thuộc nó kéo theo. Đo bằng
   `idf.py size-components` trước và sau.
3. **Hỏng thì sao?** Nó có timeout không? Có `portMAX_DELAY` ở trong không? Có tự tạo task
   không? Nhiều thư viện Arduino-style vi phạm hết các quy tắc ở `esp32-04-driver-development`.

Thư viện tốt nhưng vi phạm quy tắc → bọc nó trong một component của mình, chỉ lộ ra API đúng
hợp đồng, và ghi rõ giới hạn trong `README.md` của component đó.

## Giữ nguồn khi thư viện biến mất

Component quan trọng mà chỉ có trên GitHub của một cá nhân là rủi ro thật.
Với dự án sản phẩm: **vendor** (chép nguồn vào `components/third_party/<tên>/` kèm file ghi rõ
phiên bản gốc, URL, ngày chép, và mọi thay đổi đã làm). Đổi khả năng cập nhật tự động lấy
khả năng build được trong 5 năm nữa — với firmware, đánh đổi này gần như luôn đúng.

## Kiểm tra định kỳ

Đưa vào CI (`ci.md`): build từ **repo sạch** (clone mới, không cache) ít nhất mỗi tuần.
Đây là cách duy nhất phát hiện dependency đã trôi hoặc đã biến mất, trước khi nó xảy ra
vào đúng ngày cần phát hành bản vá khẩn cấp.
