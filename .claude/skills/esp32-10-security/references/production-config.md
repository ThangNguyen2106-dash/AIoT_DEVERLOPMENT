# Cấu hình production và tách biệt với prototype

Mục tiêu: **không thể vô tình ship bản prototype**, và không thể vô tình dùng credential
production khi đang test.

## Tách bằng file config, không tách bằng trí nhớ

```
sdkconfig.defaults              # chung cho mọi build
sdkconfig.defaults.dev          # log VERBOSE, OTA HTTP nội bộ, không ký
sdkconfig.defaults.prod         # log INFO, TLS verify, signed apps, anti-rollback
```
```bash
idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.prod" build
```

`sdkconfig` (bản sinh ra lúc build) **không commit** — nó là kết quả, và rất dễ lẫn bí mật.
`sdkconfig.defaults*` thì commit, nên tuyệt đối không đặt bí mật ở đó.

## Bảng khác biệt cần chốt

| Mục | dev | prod |
|---|---|---|
| `CONFIG_LOG_DEFAULT_LEVEL` | DEBUG/VERBOSE | INFO hoặc WARN |
| `CONFIG_BOOTLOADER_LOG_LEVEL` | INFO | WARN/ERROR |
| TLS verify | bật (vẫn phải bật) | bật + CA thật + SNTP |
| `CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT` | n | **y** |
| Anti-rollback | n | y (khi đã quản lý phiên bản) |
| Secure Boot / Flash Encryption | n hoặc Development | Release (xem cảnh báo eFuse) |
| gdbstub / JTAG | bật | tắt |
| Endpoint backend | staging | production |
| Credential | key test, quyền hạn chế | duy nhất theo thiết bị |
| `CONFIG_COMPILER_OPTIMIZATION` | Debug | Size/Performance |
| Assert | bật | cân nhắc (`CONFIG_COMPILER_OPTIMIZATION_ASSERTION_LEVEL`) |

## Cấu hình chạy được phân biệt trong firmware

```c
#if CONFIG_APP_BUILD_PROFILE_PROD
#  define BACKEND_URL  "https://api.example.com"
#else
#  define BACKEND_URL  "https://staging.example.com"
#endif
```
- Endpoint **không phải bí mật**, để trong config là được. Credential thì không.
- Hiển thị profile lúc boot: `ESP_LOGI(TAG, "build=%s ver=%s", BUILD_PROFILE, APP_VERSION);`
  — để nhìn log là biết ngay đang chạy bản nào. Đây là cách phát hiện "lỡ ship bản dev" sớm nhất.
- Chặn cứng tổ hợp sai ngay lúc biên dịch:

```c
#if CONFIG_APP_BUILD_PROFILE_PROD && defined(ALLOW_INSECURE_TLS)
#  error "prod build must not allow insecure TLS"
#endif
```

## CI: nơi bí mật production được nạp

- Khoá ký, credential, chứng chỉ nằm trong secret store của CI, đưa vào job dưới dạng biến môi
  trường hoặc file tạm — **không** nằm trong repo, không in ra log CI.
- Job build prod và job build dev tách riêng; job dev không được truy cập secret prod.
- Artifact build prod (ảnh đã ký) lưu kèm: commit hash, phiên bản, khoá nào đã ký, ngày build.
  Không truy được ảnh nào ký bằng khoá nào là không hỗ trợ được khi sự cố.
- Thêm bước quét bí mật vào CI (gitleaks / `grep` pattern ở `secrets.md`), fail build khi trúng.

## Kiểm tra trước khi ship

```bash
strings build/app.bin | grep -iE "password|api[_-]?key|token|BEGIN .*PRIVATE KEY"
grep -rn "skip_cert\|INSECURE\|SKIP_SERVER_CERT" .
idf.py size                      # đảm bảo còn vừa slot OTA
git status --porcelain           # build từ cây sạch, không có thay đổi local
```
Bản ship phải build từ commit đã tag, trên máy CI, không phải từ máy cá nhân đang sửa dở.

## Provisioning lúc sản xuất

Mỗi thiết bị cần được nạp: NVS bí mật riêng (`secrets.md`), chứng chỉ/khoá riêng nếu dùng mTLS,
số seri, và được đăng ký với backend. Đây là một quy trình, không phải một lệnh flash —
chi tiết nạp hàng loạt ở `esp32-12-release`, phần bí mật ở đây:

- Máy sản xuất giữ khoá là mục tiêu tấn công. Giới hạn quyền, ghi log ai nạp lô nào.
- Nạp xong phải **kiểm chứng**: đọc lại số seri, kiểm thiết bị kết nối được backend bằng chính
  danh tính vừa nạp. Không kiểm là ship cả lô hỏng.
- Có danh sách thiết bị đã cấp (số seri ↔ danh tính) để thu hồi được khi mất/bị chiếm.
