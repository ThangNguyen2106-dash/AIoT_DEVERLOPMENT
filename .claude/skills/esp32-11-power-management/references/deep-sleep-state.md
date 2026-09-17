# Giữ trạng thái qua deep sleep — RTC memory, NVS, thời gian

Deep sleep = reboot. Mọi biến toàn cục, heap, task, kết nối đều mất. Thiết kế giữ trạng thái
sai là nguyên nhân chính khiến thiết bị ngủ sâu hoạt động **kém tin cậy hơn** thiết bị chạy liên
tục — đó là đánh đổi thuộc trục Reliability và phải được xử lý, không phải chấp nhận im lặng.

## Ba nơi chứa trạng thái

| Nơi | Sống qua | Mất khi | Số lần ghi | Dùng cho |
|---|---|---|---|---|
| `RTC_DATA_ATTR` (RTC slow mem) | deep sleep, SW reset | mất nguồn, hibernation, một số reset | không giới hạn | boot count, BSSID, buffer tạm, trạng thái chu kỳ |
| `RTC_NOINIT_ATTR` | như trên, **không** bị xoá khi khởi động | như trên | không giới hạn | dữ liệu muốn giữ qua cả panic/SW reset |
| NVS (flash) | mọi thứ, kể cả mất nguồn | xoá flash | **có giới hạn** (~10⁴–10⁵ chu kỳ xoá/sector) | cấu hình, hiệu chuẩn, số liệu tổng |

Dung lượng RTC slow memory nhỏ (thường vài KB, tuỳ chip và tuỳ phần ULP chiếm) —
tra datasheet target, không giả định.

```c
RTC_DATA_ATTR static uint32_t s_boot_count;
RTC_DATA_ATTR static uint8_t  s_bssid[6];
RTC_DATA_ATTR static uint8_t  s_channel;
RTC_NOINIT_ATTR static uint32_t s_crash_marker;   /* giữ qua cả panic để chẩn đoán */
```

## Dữ liệu RTC memory phải được kiểm tra tính hợp lệ

Sau power-on, brownout, hay panic, nội dung RTC memory là **rác**, không phải giá trị cũ. Dùng nó
mà không kiểm tra là nguồn của những lỗi kiểu "thỉnh thoảng thiết bị gửi dữ liệu vô lý".

```c
#define RTC_MAGIC 0x50574D31u

typedef struct {
    uint32_t magic;
    uint32_t seq;
    uint16_t samples[24];
    uint8_t  count;
    uint32_t crc;          /* CRC của phần còn lại */
} rtc_state_t;

RTC_DATA_ATTR static rtc_state_t s_state;

static bool rtc_state_valid(void)
{
    return s_state.magic == RTC_MAGIC && s_state.crc == crc32_of(&s_state);
}
```

Luật: **wake cause `UNDEFINED` ⇒ coi RTC memory là không hợp lệ**, bất kể magic có đúng hay không
(magic có thể trùng ngẫu nhiên, và quan trọng hơn: reset không mong đợi nghĩa là logic trước đó
có thể đã sai).

## Thời gian qua deep sleep

RTC timer vẫn chạy khi ngủ, nên `esp_timer_get_time()` **không** reset về 0 — nhưng nó là thời
gian từ lúc cấp nguồn, không phải thời gian thực. Với thời gian thực (timestamp cho dữ liệu đo):

```c
struct timeval tv;
gettimeofday(&tv, NULL);          /* IDF giữ thời gian hệ thống qua deep sleep */
```

Ràng buộc phải biết:
- RTC clock mặc định (RC nội) **trôi đáng kể** — hàng chục giây mỗi ngày, tệ hơn theo nhiệt độ.
  Thiết bị ngủ dài mà gắn timestamp bằng RTC nội sẽ có dữ liệu lệch dần.
- Dùng crystal 32.768 kHz ngoài (`CONFIG_RTC_CLK_SRC_EXT_CRYS`) nếu cần chính xác — đây là
  quyết định **phần cứng**, phải có sẵn linh kiện trên board.
- Đồng bộ SNTP tốn thời gian radio ⇒ tốn pin. Đồng bộ định kỳ (ví dụ mỗi 24 h) thay vì mỗi chu
  kỳ, và hiệu chỉnh trôi giữa các lần. Cấu hình SNTP → `esp32-05-connectivity`.
- Mất nguồn ⇒ mất thời gian hoàn toàn. Dữ liệu đo trước khi đồng bộ lại phải được đánh dấu
  "timestamp không tin cậy", **không được gửi lên như thể đúng**.

## NVS và mòn flash

Ghi NVS mỗi chu kỳ là sai lầm kinh điển của thiết bị chạy pin: vừa tốn năng lượng (xung dòng +
vài ms), vừa mòn flash. Chu kỳ 5 phút = ~105k lần ghi/năm — vượt tuổi thọ sector.

Chiến lược đúng:

| Dữ liệu | Nơi | Tần suất ghi |
|---|---|---|
| Boot count, seq, buffer mẫu | RTC memory | mỗi chu kỳ (miễn phí) |
| Số liệu tổng, giờ chạy | NVS | gom lại, ghi mỗi N chu kỳ hoặc khi sắp mất nguồn |
| Cấu hình, hiệu chuẩn | NVS | chỉ khi thay đổi |
| Dữ liệu đo chờ gửi | RTC memory, hoặc flash riêng nếu nhiều | RTC trước, chỉ hạ xuống flash khi RTC đầy |

Chi tiết NVS, wear và schema → `esp32-06-application-development/references/storage-nvs.md`.

## Buffer dữ liệu chưa gửi

Gom nhiều mẫu rồi gửi một lần tiết kiệm rất nhiều điện (pha radio là khoản lớn nhất), nhưng đổi
lấy **rủi ro mất dữ liệu**: mất nguồn ⇒ mất cả buffer RTC.

Phải công bố rõ và cho người dùng chọn:

| Phương án | Power | Reliability |
|---|---|---|
| Gửi từng mẫu ngay | tốn nhất | mất nhiều nhất là 1 mẫu |
| Gom trong RTC memory, gửi mỗi N mẫu | tiết kiệm nhất | mất tới N mẫu nếu mất nguồn |
| Gom trong flash | trung gian (thêm chi phí ghi + mòn flash) | gần như không mất |

Với dữ liệu quan trọng (cảnh báo, số liệu tính tiền, dữ liệu theo quy định) thì gom trong RAM
không được phép — đây là ranh giới **Functionality**, không phải tinh chỉnh.

Cơ chế buffer/queue offline khi mất mạng thuộc `esp32-05-connectivity`; ở đây chỉ quyết định
buffer nằm ở đâu và sống qua cái gì.

## Khôi phục kết nối nhanh

Ghim thông tin kết nối vào RTC memory để bỏ qua quét Wi-Fi và DHCP (khoản tốn nhất trong pha
radio):

```c
RTC_DATA_ATTR static uint8_t  s_bssid[6];
RTC_DATA_ATTR static uint8_t  s_channel;
RTC_DATA_ATTR static uint32_t s_ip, s_gw, s_mask;   /* IP tĩnh từ lần trước */
```

**Bắt buộc có fallback**: AP đổi kênh, đổi thiết bị, hoặc chuyển sang AP khác trong mesh thì
thông tin ghim sai và kết nối sẽ thất bại. Logic đúng:

1. Thử kết nối nhanh bằng BSSID + kênh đã ghim, timeout ngắn (ví dụ 3 s).
2. Thất bại → xoá thông tin ghim, quét đầy đủ, kết nối bình thường.
3. Thành công → ghim lại giá trị mới.
4. Đếm số lần phải fallback và **báo lên telemetry** — tỉ lệ fallback cao nghĩa là môi trường
   đã đổi và ngân sách pin không còn đúng.

Thiếu bước 2 là bug nặng: thiết bị ngoài hiện trường sẽ không bao giờ kết nối lại được nữa sau
khi AP đổi kênh, mà vẫn ngốn pin thử mỗi chu kỳ.

## Trạng thái cơ cấu chấp hành

Chân GPIO mất trạng thái khi ngủ trừ khi giữ lại:

```c
gpio_hold_en(VALVE_GPIO);        /* giữ mức chân qua deep sleep */
gpio_deep_sleep_hold_en();
/* sau khi thức: */
gpio_hold_dis(VALVE_GPIO);       /* nhả trước khi điều khiển lại */
```

Quy tắc an toàn (bất biến toàn package): **không deep sleep khi cơ cấu chấp hành đang ở trạng
thái nguy hiểm** — van mở, bơm chạy, heater bật. Hoặc đưa về an toàn trước khi ngủ, hoặc giữ
khoá chống ngủ. Giữ mức bằng `gpio_hold_en` cho một tải nguy hiểm chỉ hợp lệ khi thiết kế đã
lường trước và có cơ chế độc lập để tắt (timer phần cứng, watchdog ngoài).

Logic fail-safe của cơ cấu chấp hành → `esp32-06-application-development`.
