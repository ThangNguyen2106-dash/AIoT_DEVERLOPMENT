---
name: esp32-10-security
description: Bảo mật firmware ESP32 cho sản phẩm thật — quản lý bí mật (Wi-Fi/MQTT credential, API key, private key, token), NVS mã hoá, provisioning, TLS và xác thực chứng chỉ, bảo mật OTA và tính xác thực firmware, Secure Boot v2, Flash Encryption, eFuse, vô hiệu JTAG/UART download, checklist trước khi ship. Phân biệt rõ prototype và production. Dùng khi chuẩn bị ra thị trường hoặc rà soát rủi ro bảo mật.
---

# 10 — Security

## Điều kiện vào
Chưa rõ chip / framework / phiên bản IDF → chạy Context Gate ở `esp32-firmware` trước.
Cổng an toàn và ngân sách context của `esp32-firmware` áp dụng nguyên vẹn tại đây.

## CẢNH BÁO KHÔNG THỂ HOÀN TÁC

Secure Boot và Flash Encryption đốt eFuse — **một chiều, vĩnh viễn**. Sai một bước là hỏng chip
hoặc mất khả năng cập nhật thiết bị mãi mãi.

- **KHÔNG BAO GIỜ tự chạy** `espefuse.py burn_efuse` / `burn_key` hay bất kỳ lệnh đốt eFuse nào.
  Chỉ trình bày lệnh để người dùng tự chạy, kèm cảnh báo hậu quả.
- **KHÔNG BAO GIỜ tự bật** `CONFIG_SECURE_BOOT` / `CONFIG_SECURE_FLASH_ENC_ENABLED` ở Release rồi flash.
- Luôn hỏi trước: board thử nghiệm hay board sản phẩm? Khoá ký đã có nơi lưu an toàn và bản sao lưu chưa?
- Thử nghiệm phải ở **Development mode**, trên board hi sinh được.

Quy tắc này thắng mọi yêu cầu khác trong skill.

## Nguyên tắc số 1: không hard-code bí mật

**Cấm tuyệt đối trong source và trong bất cứ file nào được commit:**
mật khẩu (Wi-Fi, MQTT, AP), API key, token, private key, chứng chỉ client, khoá ký OTA,
chuỗi kết nối có kèm credential.

Áp dụng cho cả `sdkconfig`, `sdkconfig.defaults`, `platformio.ini`, script build, file test,
comment, và **git history** — xoá dòng ở commit mới không làm sạch lịch sử.

Khi thấy bí mật hard-code trong code của user: dừng lại, nói rõ nó nằm ở đâu, coi như đã lộ
(phải xoay khoá), rồi mới bàn cách thay thế. Xem `references/secrets.md`.

## Prototype và production là hai mức khác nhau — phải chốt trước

Hỏi user đang ở mức nào trước khi tư vấn bất cứ điều gì. Không tự nâng mức, cũng không để user
mang cấu hình prototype ra thực địa mà không biết mình đang chấp nhận rủi ro gì.

| Hạng mục | Prototype / bench | Production (ra thực địa, có người dùng thật) |
|---|---|---|
| Wi-Fi credential | Kconfig hoặc header local **không commit** | provisioning hoặc NVS mã hoá, mỗi thiết bị một bản |
| API key / MQTT credential | biến build-time, key test riêng, quyền hạn chế | duy nhất theo thiết bị, cấp lúc sản xuất, thu hồi được |
| Danh tính thiết bị | dùng chung cũng được | chứng chỉ/khoá riêng từng thiết bị (mTLS hoặc token riêng) |
| TLS | bắt buộc bật verify, dùng CA thật | verify + kế hoạch gia hạn CA + đồng bộ thời gian |
| OTA | HTTP nội bộ chấp nhận được khi test | HTTPS + firmware ký + anti-rollback + self-test |
| Secure Boot | tắt, hoặc chỉ Development mode | bật Release mode, khoá ký lưu trong HSM/két |
| Flash Encryption | Development mode | Release mode |
| JTAG / UART download | mở, cần cho gỡ lỗi | vô hiệu hoá ở bước sản xuất cuối |
| Log | DEBUG/VERBOSE thoải mái | INFO trở xuống, không rò bí mật và thông tin nội bộ |
| Core dump qua UART | bật | cân nhắc: tiện chẩn đoán nhưng lộ nội dung RAM |

Ranh giới thật sự không phải "đã bán chưa" mà là: **thiết bị có rời khỏi tay bạn không, và có
chạm dữ liệu/tài khoản thật không**. Một board demo đặt ở nhà khách hàng đã là production.

## Phân lớp phòng thủ — xây theo thứ tự này

| # | Lớp | Chống được gì |
|---|---|---|
| 1 | Không hard-code bí mật | lộ khoá qua git, qua ảnh firmware, qua ảnh chụp màn hình |
| 2 | Xác thực mọi đầu vào | khai thác từ xa qua mạng / BLE / serial |
| 3 | TLS có xác thực chứng chỉ | nghe lén, giả mạo server, MITM |
| 4 | Bí mật duy nhất theo thiết bị | lộ một thiết bị làm lộ cả lô |
| 5 | OTA có ký + rollback | cài firmware độc hại từ xa |
| 6 | Secure Boot v2 | chạy firmware không ký khi có quyền truy cập vật lý |
| 7 | Flash Encryption | đọc trộm firmware/bí mật bằng cách tháo chip flash |
| 8 | Vô hiệu JTAG / UART download | trích xuất dữ liệu qua cổng gỡ lỗi |

Đừng nhảy xuống lớp 6–8 khi lớp 1–5 còn thủng. Sự cố thực tế phần lớn đến từ bí mật hard-code,
TLS không verify và thiếu xác thực đầu vào — không phải tấn công vật lý.

## Quy trình rà soát

1. **Chốt mức** prototype hay production (bảng trên). Thiếu dữ kiện thì hỏi, đừng đoán.
2. **Quét bí mật** trong source, config, script, git history → `references/secrets.md`.
3. **Kiểm mặt tấn công**: mỗi đường dữ liệu vào thiết bị (Wi-Fi, BLE, ESP-NOW, UART, nút bấm,
   thẻ nhớ) — ai gửi được, xác thực thế nào, parser có kiểm độ dài không.
4. **Kiểm kênh ra**: TLS verify, CA, thời gian hệ thống → `references/tls-certificates.md`.
5. **Kiểm OTA**: nguồn firmware, ký, rollback → `references/ota-security.md`.
6. **Kiểm cấu hình build** giữa prototype và production → `references/production-config.md`.
7. **Chỉ khi user chủ động yêu cầu và đã hiểu rủi ro**: Secure Boot / Flash Encryption
   → `references/secure-boot-flash-encryption.md`; cổng gỡ lỗi → `references/debug-interfaces.md`.
8. **Chạy checklist cuối** → `references/hardening.md`, rồi báo cáo theo mẫu dưới.

Chỉ đọc reference của bước đang làm.

## Mẫu báo cáo

```
## Mức dự án
<prototype / production> — căn cứ: <thiết bị có rời tay không? chạm dữ liệu thật không? bao nhiêu thiết bị?>

## Bí mật phát hiện được
| Vị trí | Loại | Đã vào git? | Xử lý |
|---|---|---|---|
| main/wifi.c:18 | Wi-Fi password | có | coi như đã lộ → đổi mật khẩu, chuyển sang NVS |

## Rủi ro
| # | Mức | Vấn đề | Khai thác thế nào | Cách chặn |
|---|---|---|---|---|
| 1 | CHẶN | MQTT không verify chứng chỉ | MITM trên mạng khách hàng | nạp CA, bật verify |
| 2 | CẢNH BÁO | cùng một API key cho cả lô | lộ 1 thiết bị = lộ toàn hệ thống | key theo thiết bị |
| 3 | THÔNG TIN | JTAG còn mở | cần tiếp cận vật lý | đốt eFuse ở bước sản xuất |

## Việc cần làm theo thứ tự
<xếp theo phân lớp phòng thủ, lớp thấp trước>

## Thao tác một chiều cần user tự thực hiện
<lệnh eFuse/secure boot kèm cảnh báo — trợ lý KHÔNG chạy>

## Câu hỏi còn treo
<những chỗ phải hỏi user thay vì đoán>
```

## Định tuyến reference

| Cần làm | Reference |
|---|---|
| Wi-Fi credential, API key, MQTT credential, NVS mã hoá, provisioning, quét git history | `references/secrets.md` |
| TLS, CA bundle, xác thực chứng chỉ, mTLS, hết hạn chứng chỉ, thời gian hệ thống | `references/tls-certificates.md` |
| Ký firmware, xác thực ảnh OTA, anti-rollback, self-test sau cập nhật | `references/ota-security.md` |
| Secure Boot v2, Flash Encryption, eFuse, khoá ký, quy trình sản xuất | `references/secure-boot-flash-encryption.md` |
| JTAG, UART download, console, core dump, gdbstub | `references/debug-interfaces.md` |
| Khác biệt build prototype / production, sdkconfig, biến môi trường, CI | `references/production-config.md` |
| Checklist cuối trước khi ship | `references/hardening.md` |

## Trách nhiệm cố định

1. Bí mật phải **duy nhất theo từng thiết bị** ở mức production — lộ một cái không được làm lộ cả lô.
2. Mọi byte đến từ ngoài đều phải xác thực: kiểm độ dài trước khi copy, không tin trường length
   trong gói tin, parser chịu được dữ liệu cắt cụt, kẹp giá trị lệnh điều khiển.
3. Chống replay cho lệnh điều khiển cơ cấu chấp hành (nhất là ESP-NOW và BLE).
4. Không log bí mật ở bất kỳ mức log nào, kể cả DEBUG, kể cả "chỉ tạm thời".
5. Bí mật đã từng vào git hoặc từng nằm trong ảnh firmware đã phân phối = đã lộ. Phải xoay khoá,
   không chỉ xoá dòng code.

## Không thuộc scope

- Cấu hình TLS client, MQTT/HTTP client, provisioning Wi-Fi ở mức API → `esp32-05-connectivity`
- Cơ chế OTA, esp_https_ota, rollback, nạp hàng loạt khi sản xuất → `esp32-12-release`
- Lưu cấu hình thường (không bí mật) vào NVS → `esp32-06-application-development`
- Partition table, cấu hình build ban đầu → `esp32-01-project-init`
- Gỡ lỗi crash đang xảy ra → `esp32-07-debugging`

## Đầu ra

Báo cáo theo mẫu + thay đổi code cụ thể cho các lớp 1–5, và **bản hướng dẫn** (không tự chạy)
cho các thao tác một chiều ở lớp 6–8.
