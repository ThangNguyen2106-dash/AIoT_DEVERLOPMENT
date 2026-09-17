---
name: esp32-06-application-development
description: Viết tầng ứng dụng của firmware ESP32 — máy trạng thái nghiệp vụ, lịch trình đo và điều khiển, xử lý sự kiện, schema dữ liệu và serialize (JSON/binary), lưu cấu hình và hiệu chuẩn vào NVS, hệ thống file SPIFFS/LittleFS/SD, logic fail-safe cho cơ cấu chấp hành. Dùng khi triển khai tính năng thực tế của sản phẩm sau khi khung task và driver đã sẵn sàng.
---

# 06 — Application Development

## Điều kiện vào
Chưa rõ chip / framework / phiên bản IDF → chạy Context Gate ở `esp32-firmware` trước.
Cổng an toàn và ngân sách context của `esp32-firmware` áp dụng nguyên vẹn tại đây.

Tầng trên cùng: **firmware này làm gì cho người dùng**. Logic nghiệp vụ, trạng thái bền vững,
và dữ liệu. Đầu ra là tính năng chạy được, kèm mức kiểm chứng đã đạt.

## Nguyên tắc tối thượng: nghiệp vụ không được chạm phần cứng

Logic nghiệp vụ gọi thẳng `gpio_set_level()` hay `i2c_master_transmit()` là **không test được**,
và mọi thứ sau đó — test, tái sử dụng, port sang board khác — đều tắc.

```c
/* SAI — không test được trên host, không đổi được cảm biến */
void control_loop(void) {
    float t = sht3x_read_temp(dev);
    if (t > 30.0f) gpio_set_level(FAN_PIN, 1);
}

/* ĐÚNG — hàm thuần: cùng input cho cùng output, không chạm I/O */
fan_cmd_t control_decide(const sensor_reading_t *in, const config_t *cfg, ctrl_state_t *st);
```

Tầng ứng dụng **nhận dữ liệu và trả quyết định**. Việc đọc cảm biến và bật quạt do tầng gọi
thực hiện. Đây là điều kiện tiên quyết của `esp32-08-testing` — không có nó thì không có host test.

## 5 trách nhiệm

1. **Máy trạng thái nghiệp vụ** dưới dạng code thuần: input là dữ liệu + cấu hình + trạng thái
   hiện tại, output là trạng thái mới + danh sách hành động. Không gọi driver, không gọi mạng,
   không `vTaskDelay`.
2. **Schema dữ liệu** (telemetry, lệnh, cấu hình) **có version**, để thiết bị cũ và server mới
   vẫn hiểu nhau → `references/data-schema.md`.
3. **Lưu trữ bền vững**: cấu hình, hiệu chuẩn, bộ đếm vào NVS; có giá trị mặc định an toàn cho
   lần chạy đầu và đường migrate khi schema đổi → `references/storage-nvs.md`.
4. **Hệ thống file** khi cần lưu nhiều/lớn (log, buffer offline, tài nguyên) →
   `references/filesystem.md`.
5. **Fail-safe**: mất cảm biến, mất lệnh, giá trị ngoài dải, mất mạng → thiết bị về trạng thái
   an toàn **theo định nghĩa nghiệp vụ** → `references/fail-safe.md`.

## Ranh giới với driver — hay nhầm nhất

| Việc | Ai làm |
|---|---|
| Đọc thanh ghi, chuyển raw → đơn vị vật lý | driver (`esp32-04`) |
| Kẹp giá trị trong **giới hạn vật lý an toàn** của thiết bị | driver |
| Ngưỡng cảnh báo, lịch đo, quyết định bật/tắt | **ứng dụng (ở đây)** |
| Thời gian timeout của **bus** | driver |
| Thời gian timeout của **nghiệp vụ** ("mất lệnh 30 s thì dừng bơm") | **ứng dụng** |
| Retry một giao dịch bus | driver |
| Quyết định làm gì khi driver đã hết cách | **ứng dụng** |

Nguyên tắc: driver bảo vệ **phần cứng**, ứng dụng bảo vệ **quy trình**. Cả hai đều cần.

## Quy trình 6 bước

| # | Bước | Đầu ra |
|---|---|---|
| 1 | **Mô tả hành vi** | thiết bị làm gì, theo lịch nào, phản ứng với sự kiện nào |
| 2 | **Tách thuần / không thuần** | hàm nào là logic thuần (test host được), hàm nào chạm I/O |
| 3 | **State machine** | danh sách state, sự kiện, bảng chuyển, hành động khi vào/ra state |
| 4 | **Schema + lưu trữ** | cấu trúc dữ liệu có version; cái gì vào NVS, cái gì vào file, cái gì chấp nhận mất |
| 5 | **Fail-safe** | với mỗi nguồn lỗi: thiết bị về đâu, sau bao lâu, báo ra ngoài thế nào |
| 6 | **Test** | host test cho bước 2-3; chỉ ra phần nào cần phần cứng thật |

## Bảng fail-safe bắt buộc

Mọi tính năng có chạm cơ cấu chấp hành phải điền đủ bảng này **trước khi viết code**:

| Nguồn lỗi | Phát hiện bằng | Sau bao lâu | Thiết bị về đâu | Báo ra ngoài |
|---|---|---|---|---|
| Cảm biến không phản hồi | driver trả ERROR 3 lần liên tiếp | 15 s | bơm dừng | LED đỏ + telemetry |
| Giá trị ngoài dải vật lý | kiểm dải ở tầng ứng dụng | ngay | bỏ mẫu, giữ lệnh cũ | đếm, log W |
| Mất mạng | `net_is_ready()` false | 5 phút | tiếp tục điều khiển cục bộ | buffer, log |
| Mất lệnh từ server | không có lệnh mới | 30 s | về chế độ tự động an toàn | telemetry |
| Mất điện giữa chừng | reset reason | — | boot vào trạng thái an toàn | crash counter |

Ô trống trong bảng này = một cách thiết bị hỏng mà chưa ai nghĩ tới.

## Mẫu báo cáo

```
## Hành vi
<mô tả một đoạn: thiết bị làm gì, theo lịch nào>

## Tách lớp
Thuần (host test được): <danh sách hàm>
Chạm I/O:               <danh sách hàm>

## State machine
| State | Vào khi | Ra khi | Hành động |
|---|---|---|---|

## Dữ liệu
| Dữ liệu | Nơi lưu | Version | Mặc định lần đầu | Sống qua OTA? |
|---|---|---|---|---|

## Fail-safe
<bảng 5 cột ở trên>

## Mức kiểm chứng
<HOST: N test đã chạy / TARGET: đã chạy trên board / phần nào còn cần phần cứng>
```

## References
- `references/state-machines.md` — viết state machine nghiệp vụ test được trên host
- `references/data-schema.md` — schema có version, JSON vs nhị phân, tương thích ngược
- `references/storage-nvs.md` — NVS, wear, namespace, quy tắc chống mất dữ liệu
- `references/filesystem.md` — LittleFS / SPIFFS / SD, khi nào dùng, chống hỏng khi mất điện
- `references/fail-safe.md` — trạng thái an toàn, giám sát, degrade có trật tự

## Không thuộc scope
- Đọc/ghi thanh ghi ngoại vi, kẹp giới hạn vật lý → `esp32-04-driver-development`
- Truyền dữ liệu đi đâu, giao thức nào, buffering offline → `esp32-05-connectivity`
- Bao nhiêu task, priority, cơ chế đồng bộ → `esp32-03-firmware-architecture`
- Xác thực dữ liệu đến từ ngoài để chống khai thác, lưu credential → `esp32-10-security`
- Chu kỳ ngủ giữa hai lần đo → `esp32-11-power-management`
- Viết test tự động → `esp32-08-testing`

## Đầu ra
Tính năng chạy được + bảng fail-safe đã điền đủ + ghi rõ phần nào đã test trên host
và phần nào cần phần cứng thật.
