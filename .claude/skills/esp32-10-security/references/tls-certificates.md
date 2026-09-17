# TLS và xác thực chứng chỉ

Mục này nói về **mặt bảo mật**. Cách cấu hình client MQTT/HTTP → `esp32-05-connectivity`.

## Luật

TLS không xác thực chứng chỉ **không bảo vệ gì cả** trước MITM — chỉ chống nghe lén thụ động.
Mọi kết nối ra ngoài phải verify chứng chỉ server, kể cả khi đang test trên mạng "nội bộ an toàn".

Cấm để sót trong code ship:

```c
.skip_cert_common_name_check = true      /* bỏ kiểm tên miền */
.crt_bundle_attach = NULL                /* và không có cert_pem → không verify */
.use_secure_element = false, .cert_pem = NULL
esp_tls_conn_new_sync(...)               /* với cfg không có CA */
```
```
CONFIG_ESP_TLS_INSECURE=y
CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY=y     ← không bao giờ ở production
CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_NONE
```

Quét: `grep -rn "skip_cert\|INSECURE\|SKIP_SERVER_CERT\|VERIFY_NONE" .`

## Hai cách cấp CA

**1. Certificate bundle** (khuyến nghị cho server công cộng — AWS, Azure, Google, Let's Encrypt):

```
CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y
CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_CMN=y
```
```c
esp_mqtt_client_config_t cfg = {
    .broker.address.uri = "mqtts://broker.example.com:8883",
    .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
};
```
Bundle cập nhật theo bản IDF; broker tự ký hoặc CA riêng thì bundle không dùng được.

**2. CA cụ thể** (broker riêng, CA nội bộ) — nhúng đúng CA đó, không nhúng cả rừng CA:

```cmake
idf_component_register(... EMBED_TXTFILES "certs/broker_ca.pem")
```
```c
extern const uint8_t ca_pem_start[] asm("_binary_broker_ca_pem_start");
cfg.broker.verification.certificate = (const char *)ca_pem_start;
```

Chứng chỉ CA **không phải bí mật** — nhúng vào firmware là bình thường. Private key thì tuyệt đối không.

## Thời gian hệ thống là điều kiện bắt buộc

Xác thực chứng chỉ kiểm tra hạn dùng. Đồng hồ sai (mặc định 1970 sau mỗi lần mất điện) làm
mọi kết nối TLS fail với lỗi khó hiểu (`ESP_ERR_MBEDTLS_SSL_HANDSHAKE_FAILED`, `-0x2700`).

Thứ tự bắt buộc: Wi-Fi lên → SNTP đồng bộ → mới mở TLS. Có RTC ngoài hoặc lưu thời gian vào
RTC memory thì rút ngắn được, nhưng vẫn phải đồng bộ lại.

## Hết hạn chứng chỉ — rủi ro vận hành lớn nhất

Thiết bị ngoài thực địa chết hàng loạt vì CA hết hạn là sự cố có thật, không hiếm.

- Ghi lại ngày hết hạn của CA đang nhúng và đặt nhắc **trước 6 tháng**.
- Phải có đường cập nhật CA qua OTA. Firmware chỉ nhúng một CA cứng, không cập nhật được,
  là bẫy hẹn giờ.
- Nhúng được cả CA cũ và CA kế nhiệm thì nhúng cả hai trong giai đoạn chuyển tiếp.
- Đừng chỉ nhúng chứng chỉ **lá** của server (pinning chặt) trừ khi có quy trình xoay rõ ràng —
  server đổi chứng chỉ mỗi 90 ngày (Let's Encrypt) sẽ làm chết thiết bị.

## mTLS — danh tính thiết bị

Production nên dùng chứng chỉ client thay cho username/password:

```c
cfg.credentials.authentication.certificate = (const char *)client_crt_start;
cfg.credentials.authentication.key = (const char *)client_key_start;
```

- Mỗi thiết bị một chứng chỉ + private key riêng. Private key nạp qua NVS mã hoá hoặc nằm trong
  secure element (ATECC608, ESP32-S3 DS peripheral) — không nhúng vào ảnh firmware dùng chung.
- Phải có cơ chế **thu hồi** (CRL, hoặc disable từ phía cloud) cho thiết bị bị mất/bị chiếm.
- ESP32 có Digital Signature peripheral (S2/S3/C3 trở lên): private key nằm trong eFuse đã mã hoá,
  phép ký chạy trong phần cứng, firmware không bao giờ thấy khoá. Đây là lựa chọn tốt nhất khi
  chip hỗ trợ.

## RAM và cấu hình mbedTLS

- Mỗi kết nối TLS tốn khoảng 20–45 KB heap lúc handshake. Hai kết nối đồng thời (MQTT + OTA)
  hay làm hết heap → crash lúc OTA. Test kịch bản này, xem `esp32-09-performance-optimization`.
- Giảm `CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN`/`OUT_CONTENT_LEN` (ví dụ 4096) khi server hỗ trợ
  fragment nhỏ. Giảm sai gây lỗi handshake với server dùng chứng chỉ lớn.
- Bật session ticket để reconnect nhanh và tốn ít RAM hơn.
- Không tắt kiểm tra chỉ để "tiết kiệm RAM". Hết RAM thì giảm số kết nối đồng thời.

## Kiểm tra thực tế

```bash
openssl s_client -connect broker.example.com:8883 -showcerts   # xem chuỗi CA thật
```
Test MITM: dùng mitmproxy với CA tự ký — thiết bị **phải** từ chối kết nối. Nếu vẫn kết nối được
thì verify chưa bật, dù code trông có vẻ đúng.
