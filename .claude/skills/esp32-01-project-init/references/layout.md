# Cây thư mục và quy ước component

## Ranh giới duy nhất thực sự quan trọng

Không phải "bao nhiêu thư mục" mà là: **code nào biên dịch được trên máy tính mà không cần ESP-IDF.**

```
components/pump_logic/      ← KHÔNG include esp_*, driver/*, freertos/*   → host test được
components/sht3x/           ← có include esp_*                            → test trên target
main/                       ← nối dây hai thứ trên lại                    → test trên target
```

Mất ranh giới này thì `esp32-08-testing` không có gì để test trên host, và mọi thay đổi logic
đều phải nạp firmware để kiểm — vòng lặp phát triển chậm gấp nhiều lần.

Cách kiểm nhanh: `grep -rl "esp_\|freertos/" components/<logic>/` — có kết quả là đã vỡ ranh giới.

## Cấu trúc một component

```
components/sht3x/
  CMakeLists.txt
  include/sht3x.h        ← API công khai + comment hợp đồng (ai gọi, blocking?, lỗi gì)
  sht3x.c
  README.md              ← nối dây, ví dụ dùng, giới hạn đã biết
  test/                  ← Unity test chạy trên target (esp32-08-testing)
```

```cmake
idf_component_register(
    SRCS        "sht3x.c"
    INCLUDE_DIRS "include"      # chỉ thư mục header CÔNG KHAI
    PRIV_INCLUDE_DIRS "."       # header nội bộ
    REQUIRES    driver          # phụ thuộc lộ ra trong header công khai
    PRIV_REQUIRES esp_timer     # phụ thuộc chỉ dùng bên trong .c
)
```

`REQUIRES` vs `PRIV_REQUIRES`: đặt sai làm phụ thuộc lan ra toàn dự án. Quy tắc —
header công khai có include nó thì `REQUIRES`, còn lại `PRIV_REQUIRES`.

## Quy ước đặt tên

| Thứ | Quy ước | Ví dụ |
|---|---|---|
| Component | `snake_case`, theo **chức năng** hoặc **tên chip** | `sht3x`, `pump_logic`, `net_link` |
| Hàm công khai | `<component>_<động từ>` | `sht3x_read()`, `pump_step()` |
| Kiểu | `<component>_<tên>_t` | `sht3x_config_t` |
| `TAG` log | trùng tên component | `static const char *TAG = "sht3x";` |
| Kconfig | `CONFIG_<COMPONENT>_<TÊN>` | `CONFIG_PUMP_MAX_RUN_MS` |

Tên component **không** đặt theo dự án (`my_project_sensor`) — component đặt tên theo chức năng
mới dùng lại được ở dự án sau, vốn là lý do tồn tại của nó.

## `main/` chứa gì

`main` là nơi **nối dây**, không phải nơi chứa chương trình:

```c
void app_main(void) {
    /* 1. hạ tầng: NVS, event loop, netif */
    /* 2. đọc cấu hình */
    /* 3. tạo bus, tạo driver từ board_config.h */
    /* 4. tạo task, nối queue */
    /* 5. xong — không có logic nghiệp vụ ở đây */
}
```

`app_main` dài quá ~150 dòng là dấu hiệu nghiệp vụ đang rò rỉ vào `main` → tách ra component.

## `board_config.h`

```c
#pragma once
/* Board: ESP32-S3-DevKitC-1 + shield cảm biến v2. Pin map đã thẩm định: docs/pinmap.md */
#define BOARD_I2C_SDA        GPIO_NUM_8
#define BOARD_I2C_SCL        GPIO_NUM_9
#define BOARD_I2C_FREQ_HZ    400000
#define BOARD_PUMP_RELAY     GPIO_NUM_4    /* pull-down 10k ngoài — mức an toàn khi reset */
```

- **Toàn bộ** số chân của dự án nằm ở đây, không chỗ nào khác.
- Mỗi chân có comment nói ràng buộc phần cứng (pull ngoài, strapping, mức logic).
- Nhiều biến thể board → `board_config_v1.h` / `v2.h`, chọn bằng Kconfig, không `#ifdef` rải rác.
- Nội dung file này đến từ pin map đã thẩm định ở `esp32-02-hardware-analysis`, không tự bịa.

## Khi tiếp nhận dự án đang lộn xộn

Đừng dựng lại từ đầu. Theo thứ tự, dừng khi đủ dùng:

1. Gom mọi `#define` chân về một `board_config.h` — rẻ nhất, lợi ích lớn nhất.
2. Tách phần logic thuần đang dính I/O ra một component (chỉ phần cần test).
3. Chuyển driver đang nằm trong `main/` thành component.
4. Chuẩn hoá tên và `TAG`.

Bước 2 chạm kiến trúc → phải hỏi user trước (`esp32-03-firmware-architecture`, mục Ngoại lệ).
