# Secure Boot v2 và Flash Encryption

## Trước khi đọc tiếp

Mọi thứ trong file này **đốt eFuse: một chiều, không hoàn tác, không có nút undo**.

- Trợ lý **không chạy** bất kỳ lệnh nào trong file này. Chỉ trình bày để user tự chạy.
- Chưa có nơi lưu khoá ký an toàn và bản sao lưu → **chưa được làm**.
- Chưa chạy trọn quy trình ở Development mode trên board hi sinh được → **chưa được làm** ở board sản phẩm.
- Mất khoá ký = không bao giờ cập nhật được thiết bị đã bán. Đây là rủi ro kinh doanh, không chỉ kỹ thuật.

Trước khi tới đây, phải đã xong: không hard-code bí mật, TLS có verify, OTA có ký
(`CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT` — không đốt eFuse, chặn được tấn công từ xa).
Secure Boot chỉ thêm phòng thủ trước **kẻ cầm được thiết bị trong tay**.

## Hai cơ chế giải quyết hai việc khác nhau

| | Secure Boot v2 | Flash Encryption |
|---|---|---|
| Chống | chạy firmware giả/sửa đổi | đọc trộm nội dung flash |
| Không chống | đọc trộm firmware | chạy firmware giả (nếu không có Secure Boot) |
| Cơ chế | bootloader kiểm chữ ký RSA-3072/ECDSA | AES mã hoá flash bằng khoá trong eFuse |

Cần chống sao chép sản phẩm và giả mạo firmware thì phải bật **cả hai**. Bật riêng Flash Encryption
mà không có Secure Boot: kẻ tấn công không đọc được, nhưng vẫn có thể thay bootloader/app.

## Quy trình an toàn (user tự chạy)

**Bước 1 — Sinh khoá, lưu ngoài repo**
```bash
espsecure.py generate_signing_key --version 2 --scheme rsa3072 secure_boot_signing_key.pem
```
Lưu vào HSM / secret manager / két có kiểm soát. Có bản sao lưu ở nơi khác. Không commit.

**Bước 2 — Development mode, trên board hi sinh được**
```
CONFIG_SECURE_BOOT=y
CONFIG_SECURE_BOOT_V2_ENABLED=y
CONFIG_SECURE_BOOT_SIGNING_KEY="secure_boot_signing_key.pem"
CONFIG_SECURE_FLASH_ENC_ENABLED=y
CONFIG_SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT=y
```
Development mode cho phép flash lại (`--encrypt`) nhiều lần, giữ đường lùi để học quy trình.
Chạy đủ: build → flash → boot → OTA một bản mới → rollback → đọc NVS mã hoá. Có lỗi thì sửa **ở đây**.

**Bước 3 — Release mode, chỉ ở board sản phẩm, chỉ khi bước 2 sạch**
```
CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=y
CONFIG_SECURE_BOOT_INSECURE=n
CONFIG_SECURE_BOOT_ALLOW_JTAG=n
CONFIG_SECURE_BOOT_ALLOW_ROM_BASIC=n
```
Sau bước này: không đọc/ghi flash bằng esptool được nữa, UART download mode bị hạn chế, cập nhật
chỉ còn qua OTA có ký. **Firmware không OTA được = thiết bị chết khi có lỗi.** Kiểm tra OTA chạy
đúng trước khi khoá.

**Bước 4 — Kiểm tra sau khi đốt**
```bash
espefuse.py summary                 # xem eFuse đã đốt đúng chưa
idf.py monitor                      # bootloader phải báo secure boot / flash encryption enabled
```
Đưa hai lệnh này vào quy trình QA sản xuất: thiết bị không đạt thì loại, đừng xuất xưởng.

## Điều dễ hỏng nhất

- **Đốt eFuse trên board dev đang dùng để phát triển** — mất board, mất thời gian. Luôn tách board.
- **Bật ở Release mà chưa test OTA** — không còn đường nạp lại bằng UART.
- **Khoá ký nằm trong repo** hoặc trong image CI công khai — Secure Boot trở nên vô nghĩa.
- **Mỗi lô dùng khoá khác nhau mà không ghi lại lô nào dùng khoá nào** — không ký được bản cập nhật
  cho lô đó nữa.
- **Quên rằng Flash Encryption làm chậm truy cập flash** và tăng thời gian boot — đo lại nếu có
  ràng buộc thời gian khởi động.
- **NVS không tự mã hoá theo Flash Encryption**: phải bật `CONFIG_NVS_ENCRYPTION` riêng và có
  partition `nvs_keys`. Xem `secrets.md`.

## Quản lý khoá ở mức sản xuất

Phải trả lời được, viết thành tài liệu, trước khi sản xuất lô đầu:

1. Ai được truy cập khoá ký? Có bao nhiêu người? Rời công ty thì xử lý thế nào?
2. Khoá lưu ở đâu, sao lưu ở đâu, ai giữ bản sao?
3. Máy nào được phép ký? CI có ký tự động không, ai duyệt?
4. Một lô dùng khoá nào — ghi vào hồ sơ sản xuất.
5. Nghi khoá bị lộ thì làm gì? (Secure Boot v2 cho phép tối đa 3 khoá và thu hồi từng khoá —
   tận dụng: đốt sẵn nhiều khoá để còn đường xoay.)
6. Thiết bị trả bảo hành có cần đọc dữ liệu không? Sau Release mode thì không đọc được nữa —
   phải có đường chẩn đoán qua chính firmware.

Không trả lời được câu nào thì dừng, làm rõ trước — sửa sau khi đã đốt là bất khả.
