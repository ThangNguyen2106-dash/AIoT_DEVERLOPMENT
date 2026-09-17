---
name: esp32-01-project-init
description: Khởi tạo và cấu hình khung dự án firmware ESP32 trên ESP-IDF (hoặc PlatformIO dùng framework espidf) — cây thư mục và component, sdkconfig.defaults, Kconfig, partition table, CMakeLists, .gitignore, ghim phiên bản và quản lý dependency, CI build đa target. Dùng khi bắt đầu dự án ESP32 mới, khi sửa cấu hình build hay partition, hoặc khi dựng lại khung cho dự án đang lộn xộn.
---

# 01 — Project Init

## Điều kiện vào
Chưa rõ chip / framework / phiên bản IDF → chạy Context Gate ở `esp32-firmware` trước.
Cổng an toàn và ngân sách context của `esp32-firmware` áp dụng nguyên vẹn tại đây.

Mọi thứ tồn tại **trước dòng code chức năng đầu tiên**: framework, cây thư mục, hệ thống build,
cấu hình biên dịch, bố trí flash, dependency, CI. Đầu ra là một dự án `idf.py build` sạch.

## Nguyên tắc tối thượng: quyết định ở đây rất khó sửa về sau

Ba thứ ở skill này đắt hơn mọi thứ khác nếu chọn sai, vì chúng chạm tới thiết bị **đã ship**:

| Quyết định | Sửa sau khi ship |
|---|---|
| **Partition table** | thiết bị cũ không nhận được bảng mới → phải thu hồi |
| **Kích thước slot OTA** | binary lớn lên là hết đường cập nhật |
| **Phiên bản IDF** | nâng cấp giữa chừng làm vỡ API, phải test lại toàn bộ |

Ba thứ này phải chốt có căn cứ ngay từ đầu, và **chốt rộng hơn nhu cầu hiện tại**.
Mọi thứ khác (cây thư mục, tên component, mức log) sửa lúc nào cũng được — đừng bàn lâu.

## Quy trình 6 bước

| # | Bước | Đầu ra | Reference |
|---|---|---|---|
| 1 | **Chốt framework và phiên bản** | ESP-IDF vX.Y, ghim cụ thể, không để trôi | `esp32-firmware/references/idf-versions.md` |
| 2 | **Cây thư mục** | tách HAL / logic thuần / app — điều kiện để test trên host | `layout.md` |
| 3 | **Partition table** | bảng có OTA (nếu cần), coredump, nvs, storage, còn biên | `partitions.md` |
| 4 | **Cấu hình build** | `sdkconfig.defaults` + biến thể production, Kconfig của dự án | `sdkconfig-keys.md`, `kconfig.md` |
| 5 | **Dependency** | ghim phiên bản mọi thành phần ngoài | `dependencies.md` |
| 6 | **CI** | build đa target mỗi lần push, theo dõi kích thước binary | `ci.md` |

Bước 3 cần biết dự án **có OTA không** — nếu chưa rõ, hỏi. Chọn bảng không OTA rồi sau này
cần OTA là phải thu hồi thiết bị.

## Framework: chọn thế nào

| | ESP-IDF | PlatformIO (framework = espidf) | Arduino core |
|---|---|---|---|
| Bộ skill này hỗ trợ | **đầy đủ** | **đầy đủ** (cùng API) | chỉ nguyên tắc, không phải API |
| Truy cập toàn bộ ESP-IDF | có | có | một phần |
| Quản lý phiên bản | `idf.py`, ghim qua git | `platform = espressif32@x.y.z` | core version |
| Hợp với | sản phẩm thật, OTA, production | sản phẩm thật, thích IDE tích hợp | prototype nhanh, thư viện có sẵn |

**Arduino**: mọi hợp đồng API trong bộ skill này (`esp_err_t`, handle-based driver,
`net_link.h`, `pm.h`) là ESP-IDF. Với Arduino, các *nguyên tắc* vẫn đúng (timeout, phân loại lỗi,
fail-safe, không hard-code bí mật) nhưng *code mẫu thì không*. Nói rõ điều này với user
ngay từ đầu thay vì im lặng đưa API không dùng được.

## Cây thư mục tối thiểu

```
project/
  CMakeLists.txt
  sdkconfig.defaults           ← vào git
  sdkconfig.prod               ← vào git (khác biệt cho production)
  sdkconfig                    ← KHÔNG vào git (sinh ra khi build)
  partitions.csv
  main/
    main.c                     ← chỉ khởi tạo và nối dây, không chứa nghiệp vụ
    board_config.h             ← TOÀN BỘ pin và tham số board, một nơi duy nhất
  components/
    <device>/                  ← driver, dùng lại được: include/, src, CMakeLists.txt, README.md
    <app_logic>/               ← logic thuần, KHÔNG include esp_*  ← đây là phần test host được
  test/                        ← host test (chạy trên máy tính)
  .github/workflows/build.yml
```

Hai quy tắc quan trọng nhất của cây thư mục này:
- **`board_config.h` là nơi duy nhất có số chân.** `#define` chân rải rác trong driver là
  nguyên nhân số một khiến dự án không port được sang board khác.
- **Component logic thuần không được `#include "esp_*"`.** Đây là ranh giới cho phép
  `esp32-08-testing` chạy test trên host. Mất ranh giới này là mất luôn host test.

## Những thứ KHÔNG được vào git

`sdkconfig` (sinh ra, gây xung đột merge liên miên), `build/`, `.pio/`,
`managed_components/`, **khoá ký OTA**, `secure_boot_signing_key.pem`, file chứa credential.

Bí mật lỡ vào git = **đã lộ**, phải xoay khoá, không chỉ xoá dòng → `esp32-10-security`.

## Mẫu báo cáo

```
## Cấu hình chốt
Framework: <ESP-IDF 5.1.2>   Target: <esp32s3>   Module: <ESP32-S3-WROOM-1-N16R8>
Có OTA: <có/không>   Có filesystem: <LittleFS 1 MB / không>   Chạy pin: <có/không>

## Partition table
| Tên | Loại | Kích thước | Lý do |
|---|---|---|---|
| nvs | data/nvs | 24K | cấu hình + danh tính nhà máy |
| ota_0/ota_1 | app | 1.9M mỗi slot | app hiện 1.2M, biên 35% |
| coredump | data/coredump | 64K | chẩn đoán từ xa |

## Khác biệt build production
<mức log, cờ debug, endpoint — hoặc "chưa có, sẽ chốt ở esp32-12-release">

## Dependency đã ghim
<danh sách: tên, phiên bản, nguồn>

## Lệnh cho user
idf.py set-target esp32s3 && idf.py build
```

## References
- `references/layout.md` — cây thư mục, quy ước component, ranh giới test được trên host
- `references/partitions.md` — mẫu bảng partition, căn chỉnh, tính kích thước OTA
- `references/sdkconfig-keys.md` — các khoá sdkconfig hay phải đụng, kèm lý do
- `references/kconfig.md` — Kconfig của dự án, khi nào dùng thay cho `#define`
- `references/dependencies.md` — ghim phiên bản IDF và component, `idf_component.yml`
- `references/ci.md` — CI build đa target, theo dõi kích thước binary
- `templates/` — partitions.csv, platformio.ini, gitignore, board_config.h

## Không thuộc scope
- Chọn chip, kiểm chân khả dụng, xung đột ngoại vi → `esp32-02-hardware-analysis`
- Thiết kế task và kiến trúc runtime → `esp32-03-firmware-architecture`
- Nội dung driver → `esp32-04-driver-development`
- Nội dung test và cấu hình test trong CI → `esp32-08-testing`
- Cấu hình production, Secure Boot, quản lý bí mật → `esp32-10-security`
- Đánh phiên bản và build phát hành → `esp32-12-release`

## Đầu ra
Cây dự án build được (`idf.py build` sạch, không warning mới) + báo cáo theo mẫu
+ một dòng lệnh set-target/build cho user. Mức kiểm chứng tối thiểu: `BUILD`.
