# Bảo mật OTA và tính xác thực firmware

Cơ chế OTA, rollback, self-test → `esp32-12-release`. Mục này chỉ nói phần bảo mật.

## Ba câu hỏi phải trả lời được

1. **Ảnh firmware đến từ đâu?** Kênh tải có xác thực server không (TLS verify — `tls-certificates.md`).
2. **Ảnh có đúng của bạn không?** Chữ ký số, không phải chỉ checksum.
3. **Có phải bản cũ bị phát lại không?** Anti-rollback / kiểm tra phiên bản.

Thiếu câu 2 thì TLS chỉ bảo vệ đường truyền: server bị chiếm, CDN bị chiếm, hoặc khoá cấu hình
URL bị sửa là thiết bị nhận firmware của kẻ khác.

## Mức tối thiểu theo giai đoạn

| | Prototype | Production |
|---|---|---|
| Kênh tải | HTTP LAN chấp nhận được | HTTPS có verify |
| Xác thực ảnh | hash/checksum | **chữ ký số** (Signed App Verification hoặc Secure Boot v2) |
| Anti-rollback | không | bật, kèm quy hoạch `secure_version` |
| Self-test sau cập nhật | tuỳ | bắt buộc, kiểm thực chất trước `mark_app_valid` |
| Nguồn URL | hard-code khi test | cấu hình ký/kiểm soát, không nhận URL tuỳ ý từ ngoài |

## Ký firmware mà không cần Secure Boot

Đây là bước nên làm trước tiên vì **không đốt eFuse, hoàn tác được**:

```
CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT=y
CONFIG_SECURE_SIGNED_APPS_ECDSA_V2_SCHEME=y
CONFIG_SECURE_BOOT_SIGNING_KEY="secure_boot_signing_key.pem"
```

Bootloader không ép, nhưng `esp_https_ota` sẽ **từ chối ảnh không ký hoặc sai chữ ký** trước khi
kích hoạt partition mới. Chặn được toàn bộ tấn công OTA từ xa. Secure Boot (đốt eFuse) chỉ thêm
phần chống người có quyền truy cập vật lý.

Sinh khoá (user tự chạy, lưu ngoài repo):
```bash
espsecure.py generate_signing_key --version 2 --scheme rsa3072 secure_boot_signing_key.pem
```
Mất khoá này = không ký được bản cập nhật nào nữa. Sao lưu ở nơi có kiểm soát truy cập
(HSM, két, secret manager của CI). **Không commit, không để trong thư mục dự án được đóng gói.**

## Anti-rollback

Chặn kẻ tấn công ép thiết bị quay về firmware cũ có lỗ hổng đã vá.

```
CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK=y
CONFIG_BOOTLOADER_APP_SECURE_VERSION=3
CONFIG_BOOTLOADER_APP_SECURE_VERSION_SIZE_EFUSE_FIELD=32
```

- `secure_version` là số nguyên tăng dần, **tăng khi vá lỗ hổng**, không phải mỗi lần build.
  Số bit eFuse có hạn — mỗi lần tăng đốt một bit, không lấy lại được.
- Thiết bị đã lên version N thì không bao giờ chạy được ảnh version < N. Đưa nhầm số lớn vào
  bản build thử là hỏng thiết bị đó vĩnh viễn ở mức đó.
- Chỉ bật khi đã có quy trình quản lý phiên bản rõ ràng; prototype thì đừng.

## Đường OTA an toàn trong code

```c
esp_http_client_config_t http = {
    .url = ota_url,                                /* từ cấu hình đã ký/kiểm soát */
    .crt_bundle_attach = esp_crt_bundle_attach,    /* BẮT BUỘC verify */
    .timeout_ms = 20000,
    .keep_alive_enable = true,
};
esp_https_ota_config_t ota = { .http_config = &http };
```

Bắt buộc kiểm trước khi kích hoạt:
- Chữ ký hợp lệ (IDF lo khi đã bật signed apps).
- `esp_app_desc_t` của ảnh mới: `project_name` khớp, `version` mới hơn — chặn nạp nhầm firmware
  của sản phẩm khác.
- Ảnh vừa lấy về khác với ảnh đang chạy (so `app_elf_sha256`) — tránh vòng lặp cập nhật.

Không bao giờ:
- Nhận URL OTA từ payload MQTT/BLE/HTTP mà không xác thực nguồn và không kiểm tra domain.
- Bỏ verify TLS "vì server nội bộ".
- `mark_app_valid_cancel_rollback()` ngay đầu `app_main` — self-test khi đó vô nghĩa, mất luôn
  lưới an toàn rollback.

## Self-test phải kiểm thực chất

Code mẫu và cơ chế rollback → `esp32-12-release/references/ota.md`. Yêu cầu **bảo mật** ở đây:
`self_test_ok()` phải kiểm được kết nối mạng lên được, cảm biến chính trả dữ liệu hợp lệ,
NVS đọc được. Chỉ kiểm "đã boot" thì firmware hỏng chức năng vẫn được đánh dấu hợp lệ,
và lưới an toàn rollback coi như không có.

## Rò rỉ qua chính ảnh firmware

Ảnh OTA tải công khai được thì ai cũng tải và phân tích được. Đừng dựa vào "không ai biết
định dạng": mọi bí mật dùng chung nằm trong ảnh coi như công khai. Bí mật phải nằm ở NVS
mã hoá theo thiết bị, không nằm trong ảnh. Flash Encryption bảo vệ bản trên chip, **không**
bảo vệ file OTA bạn đặt trên server.
