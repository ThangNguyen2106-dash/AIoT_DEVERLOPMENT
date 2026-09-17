# Lưu trữ bền vững: NVS và hệ thống file

## Chọn nơi lưu

| Dữ liệu | Dùng |
|---|---|
| Cấu hình, hiệu chuẩn, khoá, bộ đếm | NVS |
| File tài nguyên chỉ đọc (web UI, font, âm thanh) | SPIFFS hoặc LittleFS |
| Log hoặc dữ liệu ghi nhiều, thư mục | LittleFS (chống mất điện tốt hơn SPIFFS) |
| Dữ liệu lớn, trao đổi với máy tính | thẻ SD (FAT) |

SPIFFS không có thư mục thật, hiệu năng giảm mạnh khi gần đầy, và dễ hỏng khi mất điện lúc ghi.
Dự án mới nên chọn LittleFS.

## NVS

```c
esp_err_t err = nvs_flash_init();
if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
}
ESP_ERROR_CHECK(err);
```

- Dùng namespace riêng cho từng module, không dồn hết vào một namespace.
- Luôn xử lý `ESP_ERR_NVS_NOT_FOUND` bằng giá trị mặc định — lần chạy đầu tiên chắc chắn chưa có key.
- `nvs_commit()` mới thực sự ghi xuống flash. Quên gọi là mất dữ liệu khi mất điện.
- Flash có giới hạn khoảng 10k–100k chu kỳ xoá. **Không ghi NVS trong vòng lặp chính hay
  mỗi giây.** Gom lại, chỉ ghi khi giá trị thực sự đổi, hoặc theo chu kỳ dài.
- Dữ liệu nhạy cảm (khoá, token) phải dùng NVS encryption kèm partition `nvs_keys`.
- Đổi schema dữ liệu giữa các phiên bản firmware: lưu một key `version` và viết code migrate.
  Không giả định NVS trên thiết bị cũ có cùng cấu trúc với firmware mới.

