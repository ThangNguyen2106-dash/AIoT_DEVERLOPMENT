# Quản lý bí mật

Bí mật = mật khẩu Wi-Fi/AP, MQTT username+password, API key, token, private key, chứng chỉ client,
khoá ký OTA, khoá mã hoá dữ liệu, chuỗi kết nối có kèm credential.

## Quét trước khi làm bất cứ việc gì

```bash
grep -rniE "password|passwd|pwd|secret|api[_-]?key|token|private[_-]?key|BEGIN [A-Z ]*PRIVATE KEY" \
  main/ components/ --include=*.c --include=*.h --include=*.ini --include=*.txt
grep -rn "CONFIG_.*\(PASSWORD\|KEY\|TOKEN\)" sdkconfig sdkconfig.defaults 2>/dev/null
git log -p --all -S "PRIVATE KEY" | head        # bí mật trong lịch sử
git log --all --name-only | grep -iE "\.pem|\.key|\.p12|credentials|\.env"
```

**Bí mật đã vào git, hoặc đã nằm trong ảnh firmware phát cho ai đó = đã lộ.** Xoá dòng code không
cứu được: phải **xoay khoá** (đổi mật khẩu, thu hồi và cấp lại API key, sinh lại chứng chỉ).
Nói thẳng điều này với user trước khi bàn giải pháp kỹ thuật.

Phòng ngừa: thêm `.env`, `*.pem`, `*.key`, `secrets.h`, `sdkconfig` (bản local) vào `.gitignore`
và cài pre-commit hook quét bí mật.

## Bốn cách nạp bí mật — chọn theo mức dự án

| Cách | Bí mật nằm ở đâu | Dùng khi | Hạn chế |
|---|---|---|---|
| Kconfig / header local không commit | trong ảnh firmware | prototype trên bàn | cả lô giống nhau; đọc được từ flash |
| NVS nạp lúc sản xuất (`nvs_partition_gen.py`) | partition NVS riêng | production phổ thông | phải mã hoá NVS, nếu không đọc được bằng esptool |
| Provisioning lúc dùng (SoftAP/BLE/Espressif Provisioning) | NVS, do người dùng cuối nhập | Wi-Fi credential của khách hàng | cần luồng UI; bản thân kênh provisioning phải an toàn |
| eFuse / HSM / secure element (ATECC608) | trong chip, không đọc ra được | khoá danh tính thiết bị, mTLS | tốn phần cứng hoặc eFuse block, một chiều |

Quy tắc chung: **credential của người dùng cuối thì provisioning; danh tính của thiết bị thì nạp
lúc sản xuất, duy nhất từng thiết bị.**

## Prototype: làm tối thiểu nhưng đúng hướng

```c
/* main/secrets.h — trong .gitignore, KHÔNG commit */
#define WIFI_SSID     "lab-ap"
#define WIFI_PASSWORD "..."
```
kèm `secrets.h.example` có giá trị giả để người khác biết cần gì. Hoặc dùng Kconfig với
`sdkconfig` local không commit (nhớ: `sdkconfig.defaults` thì **được** commit, nên không để bí mật ở đó).

Vẫn phải: key test riêng, quyền hạn chế, không dùng lại credential production.

## Production: NVS mã hoá

Không mã hoá thì NVS chỉ là plaintext trong flash, đọc bằng `esptool.py read_flash` là ra.

```
# partitions.csv
nvs_keys, data, nvs_keys, ,      4K,  encrypted
nvs,      data, nvs,      ,     24K,
```

Sinh partition NVS chứa bí mật (chạy ở máy sản xuất, không commit CSV có giá trị thật):

```bash
python $IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py encrypt \
    device_secrets.csv nvs_encrypted.bin 0x6000 --keygen --keyfile keys-dev-001.bin
```

- `CONFIG_NVS_ENCRYPTION=y`; khoá NVS được bảo vệ bởi Flash Encryption → hai thứ này đi cùng nhau.
- Mỗi thiết bị một `keyfile` và một ảnh NVS riêng. File khoá lưu ngoài repo, trong hệ thống có
  kiểm soát truy cập; huỷ sau khi nạp nếu quy trình cho phép.

Đọc trong firmware như NVS thường:

```c
nvs_handle_t h;
ESP_RETURN_ON_ERROR(nvs_open("prov", NVS_READONLY, &h), TAG, "nvs_open");
size_t len = sizeof(buf);
esp_err_t err = nvs_get_str(h, "mqtt_pass", buf, &len);
nvs_close(h);
/* Thiếu bí mật là lỗi cấu hình sản xuất: KHÔNG rơi về giá trị mặc định hard-code. */
if (err != ESP_OK) return ESP_ERR_INVALID_STATE;
```

## Từng loại bí mật

**Wi-Fi credential.** Của khách hàng thì provisioning, lưu NVS mã hoá, cho xoá được (reset về
xuất xưởng). Không log SSID kèm password. AP mode dùng để provisioning phải có mật khẩu mạnh,
duy nhất theo thiết bị (ví dụ dẫn xuất từ MAC + salt bí mật, không phải đúng bằng MAC),
và tắt sau khi xong.

**API key / token.** Duy nhất theo thiết bị, có thể thu hồi từ phía server, có hạn sử dụng nếu
backend hỗ trợ. Token ngắn hạn thì lưu RAM, không ghi flash. Không nhúng key của tài khoản
quản trị vào thiết bị — thiết bị chỉ được có quyền tối thiểu nó cần.

**MQTT credential.** Ưu tiên chứng chỉ client (mTLS) hơn username/password. Dùng
username/password thì mỗi thiết bị một bộ, và broker phải giới hạn topic theo từng client
(ACL) để một thiết bị bị chiếm không điều khiển được thiết bị khác.

**Private key.** Sinh **trên thiết bị** nếu chip/secure element hỗ trợ — khoá không bao giờ rời chip.
Không sinh được thì sinh ở máy sản xuất có kiểm soát, nạp một lần, không lưu lại bản sao trên
máy đó. Không bao giờ dùng chung một private key cho nhiều thiết bị.

## Trong code

- Không `ESP_LOGx` bí mật ở bất kỳ mức nào. Cần xác nhận đã nạp thì log độ dài hoặc 4 ký tự đầu
  của hash, không log giá trị.
- Xoá bí mật khỏi RAM sau khi dùng xong khi có thể (`memset` — với `volatile` hoặc
  `esp_fill_random` để trình biên dịch không tối ưu mất).
- Không đưa bí mật vào thông báo lỗi trả ra ngoài, không đưa vào core dump nếu tránh được.
- So sánh token/HMAC bằng hàm so sánh thời gian cố định, không `strcmp` (dù với ESP32 thì
  timing attack qua mạng khó, đây là thói quen rẻ tiền nên giữ).
