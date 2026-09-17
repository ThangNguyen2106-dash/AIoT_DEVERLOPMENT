# Checklist làm cứng firmware trước khi ship

Cột **P** = bắt buộc cả với prototype. Cột **PROD** = thêm vào khi thiết bị rời tay bạn hoặc
chạm dữ liệu/tài khoản thật.

## Bí mật và cấu hình

| | Mục | P | PROD |
|---|---|---|---|
| ☐ | Không có SSID, mật khẩu, API key, token, private key trong source, config, script | ✔ | ✔ |
| ☐ | Đã quét **git history**, không còn bí mật trong lịch sử | ✔ | ✔ |
| ☐ | Bí mật từng lộ đã được **xoay khoá**, không chỉ xoá dòng code | ✔ | ✔ |
| ☐ | `.gitignore` có `*.pem`, `*.key`, `secrets.h`, `.env`, `sdkconfig` | ✔ | ✔ |
| ☐ | Không log bí mật ở bất kỳ mức log nào | ✔ | ✔ |
| ☐ | `strings app.bin` không lộ bí mật | ✔ | ✔ |
| ☐ | Bí mật nạp qua NVS mã hoá hoặc provisioning, **duy nhất theo thiết bị** | | ✔ |
| ☐ | `CONFIG_NVS_ENCRYPTION=y` + partition `nvs_keys` | | ✔ |
| ☐ | Thiếu bí mật thì báo lỗi, không rơi về giá trị mặc định hard-code | | ✔ |
| ☐ | Có đường thu hồi credential của một thiết bị từ phía backend | | ✔ |

→ `secrets.md`

## Mạng và TLS

| | Mục | P | PROD |
|---|---|---|---|
| ☐ | Mọi kết nối ra ngoài dùng TLS và **có xác thực chứng chỉ server** | ✔ | ✔ |
| ☐ | Không còn `skip_cert_common_name_check`, `ESP_TLS_INSECURE`, CA tạm nào sót | ✔ | ✔ |
| ☐ | Đã test MITM bằng CA giả — thiết bị **từ chối** kết nối | ✔ | ✔ |
| ☐ | SNTP đồng bộ trước khi mở TLS (đồng hồ sai làm handshake fail) | ✔ | ✔ |
| ☐ | AP mode có mật khẩu mạnh, duy nhất theo thiết bị, tắt sau provisioning | ✔ | ✔ |
| ☐ | Web/telnet/debug server đã tắt hoặc có xác thực | ✔ | ✔ |
| ☐ | Đã ghi ngày hết hạn CA, có nhắc trước 6 tháng, có đường cập nhật CA qua OTA | | ✔ |
| ☐ | Danh tính thiết bị riêng (mTLS hoặc token riêng), broker có ACL theo client | | ✔ |

→ `tls-certificates.md`

## Đầu vào

| | Mục | P | PROD |
|---|---|---|---|
| ☐ | Mọi parser (JSON, binary, lệnh UART, đặc tính BLE ghi được) kiểm độ dài và kẹp giá trị | ✔ | ✔ |
| ☐ | Không tin trường length trong gói tin; parser chịu được dữ liệu cắt cụt | ✔ | ✔ |
| ☐ | Không `strcpy`, `sprintf`, `gets`, `strcat` trên dữ liệu từ ngoài | ✔ | ✔ |
| ☐ | Lệnh điều khiển cơ cấu chấp hành có xác thực nguồn gốc và **chống replay** | ✔ | ✔ |
| ☐ | Giới hạn tần suất lệnh từ ngoài (chống làm ngập / làm kiệt pin) | | ✔ |

## OTA

| | Mục | P | PROD |
|---|---|---|---|
| ☐ | OTA qua HTTPS đã xác thực chứng chỉ | | ✔ |
| ☐ | Ảnh firmware được **ký** (`CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT` tối thiểu) | | ✔ |
| ☐ | Kiểm `project_name` và version của ảnh mới trước khi kích hoạt | | ✔ |
| ☐ | URL OTA không nhận tuỳ ý từ payload ngoài | | ✔ |
| ☐ | Rollback bật; `self_test` kiểm **thực chất** trước `mark_app_valid` | | ✔ |
| ☐ | Đã test OTA với kịch bản mất mạng giữa chừng và mất điện giữa chừng | ✔ | ✔ |
| ☐ | Anti-rollback bật (khi đã có quy trình quản lý `secure_version`) | | ✔ |

→ `ota-security.md`

## Cổng gỡ lỗi

| | Mục | P | PROD |
|---|---|---|---|
| ☐ | Log mặc định INFO trở xuống, không rò thông tin nội bộ ra UART | | ✔ |
| ☐ | gdbstub tắt | | ✔ |
| ☐ | JTAG / USB-JTAG vô hiệu hoá (eFuse, bước sản xuất cuối) | | ✔ |
| ☐ | UART download mode vô hiệu hoá (eFuse, bước sản xuất cuối) | | ✔ |
| ☐ | Test point UART/JTAG trên PCB đã bỏ hoặc che | | ✔ |
| ☐ | Có đường chẩn đoán từ xa thay thế (core dump, thống kê tự báo) | | ✔ |

→ `debug-interfaces.md`

## Cấu hình build

| | Mục | P | PROD |
|---|---|---|---|
| ☐ | Profile dev và prod tách bằng `sdkconfig.defaults.*`, không tách bằng trí nhớ | ✔ | ✔ |
| ☐ | Firmware in ra profile + version lúc boot | ✔ | ✔ |
| ☐ | Tổ hợp cấu hình sai bị chặn ngay lúc biên dịch (`#error`) | | ✔ |
| ☐ | Bản ship build từ commit đã tag, trên CI, cây sạch | | ✔ |
| ☐ | CI có bước quét bí mật, fail build khi trúng | | ✔ |
| ☐ | Lưu vết: ảnh nào ← commit nào ← ký bằng khoá nào | | ✔ |

→ `production-config.md`

## Phần cứng — chỉ khi user chủ động yêu cầu và đã hiểu rủi ro

| | Mục |
|---|---|
| ☐ | Khoá ký Secure Boot sinh ngoài repo, lưu trong HSM/két, **có sao lưu**. Mất khoá = không bao giờ cập nhật được nữa |
| ☐ | Đã chạy trọn quy trình ở **Development mode** trên board hi sinh được |
| ☐ | Đã xác nhận OTA hoạt động **trước khi** khoá Release mode |
| ☐ | Flash Encryption Release mode — hiểu rằng không còn đọc/ghi flash bằng esptool |
| ☐ | Secure Boot v2 bật cùng Flash Encryption (bật riêng một cái là phòng thủ nửa vời) |
| ☐ | Tài liệu quy trình khoá: ai giữ, lưu đâu, máy nào ký, lô nào dùng khoá nào, xử lý khi nghi lộ |
| ☐ | `espefuse.py summary` là bước QA bắt buộc trước khi xuất xưởng |

→ `secure-boot-flash-encryption.md`

## Nhắc lại

Mọi thao tác eFuse là một chiều. Trợ lý **KHÔNG tự chạy** lệnh đốt eFuse, không tự bật
Secure Boot / Flash Encryption ở Release rồi flash — chỉ đưa lệnh để user tự thực hiện sau khi
đã xác nhận hiểu hậu quả.
