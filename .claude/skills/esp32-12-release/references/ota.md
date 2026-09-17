# OTA — cập nhật firmware từ xa

File này sở hữu **cơ chế** OTA (rollback, self-test, chống brick). Phần **bảo mật** OTA
(TLS verify, ký ảnh, anti-rollback, nguồn URL) thuộc `esp32-10-security/references/ota-security.md`.
Hai skill nói khác nhau → **`esp32-10-security` thắng**.

## Prototype và production

| | Prototype / bench | Production |
|---|---|---|
| Kênh tải | HTTP trong LAN chấp nhận được khi test | **HTTPS có verify**, bắt buộc |
| Xác thực ảnh | hash/checksum | **chữ ký số** |
| Rollback | nên bật | **bắt buộc** |
| Self-test | tuỳ | **bắt buộc**, kiểm thực chất |

Ví dụ dưới đây là cấu hình **production**. Hạ xuống prototype thì phải nói rõ đang hạ mục nào
và rủi ro đi kèm — không im lặng bỏ `crt_bundle_attach`.

Bắt buộc dùng cơ chế rollback của ESP-IDF. Bật `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`.

```c
esp_http_client_config_t http = {
    .url = fw_url,
    .crt_bundle_attach = esp_crt_bundle_attach,   /* OTA phải qua HTTPS đã xác thực */
    .timeout_ms = 20000,
    .keep_alive_enable = true,
};
esp_https_ota_config_t ota = { .http_config = &http };
esp_err_t err = esp_https_ota(&ota);
if (err == ESP_OK) {
    esp_restart();
}
```

Sau khi khởi động vào firmware mới, ứng dụng phải **tự kiểm tra sức khoẻ** rồi mới xác nhận:

```c
const esp_partition_t *run = esp_ota_get_running_partition();
esp_ota_img_states_t state;
if (esp_ota_get_state_partition(run, &state) == ESP_OK &&
    state == ESP_OTA_IMG_PENDING_VERIFY) {
    if (self_test_ok()) {
        esp_ota_mark_app_valid_cancel_rollback();
    } else {
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
}
```

`self_test_ok()` nên kiểm tra thực chất: ngoại vi quan trọng phản hồi, kết nối được server,
heap còn đủ. Không chỉ `return true`. Thiếu bước này thì rollback vô dụng và một bản firmware
hỏng sẽ làm chết cả lô thiết bị ngoài thực địa.

## Nguyên tắc chống brick

- Luôn có 2 slot OTA kích thước bằng nhau và đủ chỗ (xem `esp32-01-project-init/references/partitions.md`).
- Firmware phải được **ký** và bootloader xác thực (Secure Boot) nếu sản phẩm thương mại.
  Tối thiểu phải kiểm tra checksum/hash của ảnh tải về.
- Không cho phép OTA khi pin yếu hoặc đang điều khiển cơ cấu chấp hành quan trọng.
- Ghi lại phiên bản đang chạy và phiên bản trước vào NVS để chẩn đoán từ xa.
- Có cơ chế đếm số lần boot thất bại liên tiếp; vượt ngưỡng thì quay về firmware an toàn.
- Test OTA phải bao gồm kịch bản **mất mạng giữa chừng** và **mất điện giữa chừng**.
  Thiết bị phải vẫn boot được vào bản cũ.
