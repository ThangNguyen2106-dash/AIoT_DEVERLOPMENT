# Phân lớp

## Bốn lớp

```
app/      nghiệp vụ: cái sản phẩm này làm. Không biết I2C, không biết MQTT.
service/  năng lực dùng lại: đo đạc, đồng bộ dữ liệu, cấu hình, thời gian, cập nhật.
driver/   một ngoại vi / một chip ngoài: SHT31, relay board, LED strip.
hal/      trừu tượng hoá chip/board: đổi ESP32 → S3, hoặc thay bằng fake khi test host.
```

Phụ thuộc **một chiều, đi xuống**. `app` include header của `service`; `service` include
header của `driver`. Không bao giờ ngược. Không vòng. Cần dữ liệu đi lên → dùng callback
hoặc event, đăng ký từ trên xuống.

Không phải dự án nào cũng có đủ bốn lớp. Xem `tiers.md`.

## Trách nhiệm từng lớp

| Lớp | Được làm | Không được làm |
|---|---|---|
| app | quyết định, lịch trình, state machine sản phẩm | gọi `driver/*.h` của IDF, biết chân GPIO |
| service | gom nhiều driver, retry, cache, đơn vị đo, chính sách | chứa quy tắc nghiệp vụ riêng của sản phẩm |
| driver | thanh ghi, protocol, timing, chuyển raw → giá trị vật lý | quyết định "khi nào đo", "gửi đi đâu" |
| hal | bọc API IDF theo chân/bus của board này | chứa logic |

Phép thử nhanh: **xoá `app/` đi, `service/` + `driver/` vẫn phải compile được và vẫn có
nghĩa cho một sản phẩm khác.** Không được thì ranh giới đang sai.

## Hợp đồng interface

Mỗi component có đúng một header công khai. Trong đó phải nói rõ, bằng comment:

```c
/* sensor_svc.h — đọc nhiệt độ/độ ẩm đã hiệu chuẩn.
 *
 * Context : gọi từ task, KHÔNG gọi từ ISR.
 * Blocking: sensor_svc_read() block tối đa 120 ms (thời gian đo của cảm biến).
 * Thread  : an toàn đa task, khoá nội bộ.
 * Bộ nhớ  : caller cấp `out`; service không giữ con trỏ sau khi trả về.
 * Lỗi     : ESP_ERR_TIMEOUT (bus bận), ESP_ERR_INVALID_RESPONSE (CRC sai),
 *           ESP_ERR_INVALID_STATE (chưa init). Mọi lỗi đều phục hồi được — retry.
 */
esp_err_t sensor_svc_init(void);
esp_err_t sensor_svc_read(sensor_reading_t *out);
```

Năm dòng đó — context, blocking, thread-safety, sở hữu bộ nhớ, tập lỗi — là phần đắt nhất
của kiến trúc. Thiếu chúng thì có lớp cũng vô nghĩa.

## Quy tắc coupling

- Header công khai chỉ khai báo cái người ngoài cần. Struct nội bộ để trong `.c`
  hoặc `*_private.h`.
- Không có biến global chia sẻ giữa component. Trạng thái nằm sau hàm truy cập.
- Component không tự khởi tạo component khác. Ai sở hữu vòng đời thì người đó init —
  thường là `app_main` hoặc một `app_init()` duy nhất, theo thứ tự tường minh.
- Truyền phụ thuộc vào lúc init (`svc_init(&cfg)` nhận handle bus), không để service
  tự đi tìm bus toàn cục. Đây cũng là cái làm test được.

## Testability

Mục tiêu: **logic thuần chạy được trên host, không cần chip.**

- Tách hàm tính toán (chuyển đổi, lọc, ngưỡng, bảng chuyển trạng thái) thành file
  không include gì của ESP-IDF. Đây là phần đáng test nhất và rẻ nhất để test.
- I/O nằm ở rìa: driver gọi IDF, mọi thứ trên nó nhận dữ liệu qua tham số.
- Cần thay thế được ở ranh giới nào thì đặt struct con trỏ hàm ở đúng ranh giới đó —
  **chỉ ở đó**, không rải khắp nơi.
- Chi tiết viết test → `esp32-08-testing`.

## Khi đọc code có sẵn

Nhận diện cấu trúc đang có trước, rồi mới nói. Ba trường hợp:

1. Có phân lớp rõ → bám theo, thêm file vào đúng lớp.
2. Phẳng nhưng nhất quán (mọi thứ trong `main/`) → thêm vào theo đúng kiểu đó.
   Đây là hợp lệ ở mức simple, không phải lỗi.
3. Lộn xộn thật (include vòng, app gọi thẳng thanh ghi, global chia sẻ) → **nêu ra,
   đề xuất, không tự sửa.** Nếu buộc phải thêm code vào đó, thêm theo cách không làm
   tệ hơn: file mới đúng lớp, không thêm global mới.
