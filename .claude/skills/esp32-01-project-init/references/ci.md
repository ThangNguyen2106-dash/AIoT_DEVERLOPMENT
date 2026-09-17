# CI cho firmware ESP32

File này lo **build**. Chạy test trong CI thuộc `esp32-08-testing/references/unity-and-ci.md`.

## Vì sao firmware cần CI hơn cả phần mềm thường

Lỗi phổ biến nhất khi viết firmware đa chip: dùng một API **không tồn tại trên chip khác**
(DAC trên C3, Bluetooth trên S2, `hall_sensor_read` trên IDF v5). Không ai phát hiện được
cho tới khi có người build cho chip đó — thường là lúc đã muộn.

**Build đa target mỗi lần push là cách rẻ nhất bắt loại lỗi này.** Nó chạy trong vài phút
và không cần phần cứng.

## Khung tối thiểu

`.github/workflows/build.yml`:

```yaml
name: build
on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    strategy:
      fail-fast: false            # thấy hết target hỏng, không dừng ở cái đầu tiên
      matrix:
        target: [esp32, esp32s3, esp32c3]
    container:
      image: espressif/idf:v5.1.2  # GHIM đúng phiên bản dev đang dùng
    steps:
      - uses: actions/checkout@v4
        with: { submodules: recursive }
      - name: Build
        shell: bash
        run: |
          . $IDF_PATH/export.sh
          idf.py set-target ${{ matrix.target }}
          idf.py build
      - name: Size
        shell: bash
        run: |
          . $IDF_PATH/export.sh
          idf.py size
```

Chỉ liệt kê target **thật sự hỗ trợ**. Thêm một target vào matrix là cam kết duy trì nó.

## Warning phải là lỗi

Warning bị bỏ qua sẽ tích tụ tới mức không ai đọc nữa, và warning thật lẫn vào đó.

```cmake
# CMakeLists.txt gốc
idf_build_set_property(COMPILE_OPTIONS "-Wall;-Wextra" APPEND)
```

Dự án mới: bật `-Werror` ngay từ đầu — rẻ. Dự án đang có sẵn: **đừng bật ngay**, sẽ đỏ hàng trăm
chỗ và không ai sửa. Thay vào đó đếm số warning và **chặn khi số này tăng**, rồi giảm dần.

## Theo dõi kích thước binary

Kích thước app chỉ có tăng, và vượt slot OTA là sự cố nghiêm trọng
(`esp32-09/references/binary-size.md`). Bắt sớm:

```bash
SIZE=$(idf.py size --format json | python -c "import sys,json;print(json.load(sys.stdin)['used_app_bin_size'])")
LIMIT=1900000        # kích thước slot OTA
echo "app: $SIZE / $LIMIT"
[ "$SIZE" -lt "$LIMIT" ] || { echo "VƯỢT SLOT OTA"; exit 1; }
```

Tốt hơn nữa: cảnh báo ở **85% slot**, fail ở 100% — để còn thời gian xử lý trước khi kẹt thật.

## Những thứ nên có khi dự án lớn dần

| Việc | Khi nào thêm |
|---|---|
| Build sạch hàng tuần (clone mới, không cache) | ngay — bắt dependency trôi (`dependencies.md`) |
| Build mọi **biến thể board** trong matrix | khi có ≥ 2 biến thể (`kconfig.md`) |
| Build cả `sdkconfig.prod` | trước lần release đầu tiên |
| Host test | khi đã có component logic thuần → `esp32-08-testing` |
| Lưu artifact: `.bin`, **`.elf`**, `.map` | trước lần release đầu tiên |
| Quét bí mật trong diff | trước khi repo có nhiều người → `esp32-10-security` |
| Static analysis (`clang-tidy`, `cppcheck`) | khi dự án ổn định |
| HIL test trên board thật | khi có ngân sách runner gắn board |

**Lưu `.elf` theo từng build là bắt buộc trước khi ship.** Không có ELF đúng bản thì core dump
và backtrace từ thực địa không giải mã được (`esp32-07-debugging`,
`esp32-12-release/references/remote-diagnostics.md`).

## Bí mật trong CI

Credential để ký firmware hoặc để đẩy artifact dùng secret của CI, **không** đặt trong repo,
không in ra log. Khoá ký OTA đặc biệt nhạy: mất nó là không ký được bản cập nhật nào nữa
(`esp32-10-security/references/ota-security.md`).
