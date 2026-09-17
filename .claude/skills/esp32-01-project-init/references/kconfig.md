# Kconfig của dự án

## Khi nào dùng Kconfig thay cho `#define`

| Dùng Kconfig | Dùng `#define` trong `board_config.h` |
|---|---|
| Giá trị đổi giữa các **biến thể build** (dev/prod, board v1/v2) | pin và tham số cố định của một board |
| Giá trị người khác cần chỉnh mà không sửa code | hằng số nội bộ của một module |
| Bật/tắt tính năng cả khối | — |
| Giá trị CI cần override | — |

| Không dùng cả hai — dùng **NVS** |
|---|
| Giá trị khác nhau **từng thiết bị** (serial, hiệu chuẩn, Wi-Fi credential) |
| Giá trị người dùng cuối đổi được ở runtime |

Ba tầng này hay bị trộn. Quy tắc: **build-time → Kconfig; per-board → `board_config.h`;
per-device/runtime → NVS** (`esp32-06-application-development/references/storage-nvs.md`).

Bí mật **không bao giờ** vào Kconfig ở production — `sdkconfig` sẽ nằm trong build artifact và
rất dễ lọt vào git. Prototype thì chấp nhận được nếu file không commit
(`esp32-10-security/references/secrets.md`).

## Khai báo

`components/pump_logic/Kconfig.projbuild` (hiện ở `idf.py menuconfig`):

```kconfig
menu "Pump control"

    config PUMP_MAX_RUN_MS
        int "Thời gian chạy bơm tối đa (ms)"
        range 1000 600000
        default 120000
        help
            Bơm tự dừng sau khoảng này kể cả khi chưa đạt mức.
            Giới hạn nhiệt của bơm — xem datasheet mục Duty Cycle.

    config PUMP_ENABLE_DRY_RUN_GUARD
        bool "Chặn chạy khô"
        default y

endmenu
```

Dùng trong code:

```c
#include "sdkconfig.h"
if (elapsed_ms > CONFIG_PUMP_MAX_RUN_MS) { ... }
```

Quy tắc viết Kconfig:
- **Luôn có `range`** cho `int` — chặn được cấu hình vô nghĩa ngay lúc build.
- **`help` nói *vì sao*, không nói lại tên.** "Thời gian tối đa" là vô dụng;
  "giới hạn nhiệt của bơm, xem datasheet" mới có giá trị.
- `default` phải là **giá trị an toàn**, không phải giá trị tiện cho dev.
- `Kconfig.projbuild` hiện trong menu chính; `Kconfig` thường chỉ hiện khi component được chọn.

## Nhiều biến thể build

```
sdkconfig.defaults            # chung cho mọi biến thể
sdkconfig.defaults.esp32s3    # tự áp dụng khi target = esp32s3
sdkconfig.prod                # khác biệt production
```

```bash
idf.py build                                                  # dev
SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.prod" idf.py build   # production
```

- **`sdkconfig` không vào git.** Nó sinh ra từ các file trên; commit nó gây xung đột merge
  liên miên và làm không ai biết cấu hình thật là gì.
- Mọi khác biệt dev/production phải nằm trong **file**, không nằm trong tay người build.
  Danh sách khác biệt bắt buộc liệt kê đủ → `esp32-12-release/references/versioning.md`.
- Đổi `sdkconfig.defaults` sau khi đã có `sdkconfig` thì **không tự áp dụng** — phải xoá
  `sdkconfig` rồi build lại. Đây là nguồn của rất nhiều "tôi đổi rồi mà không thấy tác dụng".

## Biến thể board

```kconfig
choice BOARD_VARIANT
    prompt "Biến thể board"
    default BOARD_V2
    config BOARD_V1
        bool "v1 (relay active-high)"
    config BOARD_V2
        bool "v2 (relay active-low, thêm cảm biến dòng)"
endchoice
```

```c
/* board_config.h */
#if CONFIG_BOARD_V1
  #define BOARD_RELAY_ACTIVE_LEVEL 1
#else
  #define BOARD_RELAY_ACTIVE_LEVEL 0
#endif
```

`#ifdef` tập trung ở **một** file. Rải `#if CONFIG_BOARD_V1` khắp driver là cách chắc chắn
để một ngày nào đó nạp nhầm firmware v1 lên board v2 và relay đảo ngược —
với relay điều khiển tải thật, đó là sự cố an toàn, không phải bug.

Mỗi biến thể phải được **build trong CI** (`ci.md`); biến thể không build thường xuyên là
biến thể đã hỏng mà chưa ai biết.
