# Ngân sách năng lượng và đo đạc

## Mô hình năng lượng một chu kỳ

Đơn vị làm việc: **mAs** (milliampere-giây) cho từng pha. `mAs = dòng(mA) × thời gian(s)`.
Cộng lại rồi chia cho chu kỳ để ra dòng trung bình.

```
E_chu_kỳ (mAs) = Σ (I_pha × t_pha)
I_trung_bình (mA) = E_chu_kỳ / T_chu_kỳ (s)
Tuổi thọ (giờ)   = C_hiệu_dụng (mAh) / I_trung_bình (mA)
```

Ví dụ điền thật (thiết bị đo nhiệt độ, chu kỳ 5 phút = 300 s):

| Pha | I | t | mAs |
|---|---|---|---|
| boot + init (bao gồm cả thời gian bootloader) | 60 mA | 0.4 s | 24 |
| đo cảm biến (có chờ ổn định) | 25 mA | 1.2 s | 30 |
| Wi-Fi connect + TLS + publish | 120 mA | 4.5 s | 540 |
| deep sleep | 0.018 mA | 294 s | 5.3 |
| **tổng** | | 300 s | **599 mAs** |

→ `I_tb = 599/300 ≈ 2.0 mA` → với pin 2000 mAh hiệu dụng 1700 mAh: **~850 giờ ≈ 35 ngày**.

Điểm quan trọng đọc ra từ bảng: **90% năng lượng nằm ở pha radio**, deep sleep chỉ chiếm 1%.
Tối ưu dòng ngủ từ 18 µA xuống 10 µA ở đây gần như vô nghĩa; rút pha Wi-Fi từ 4.5 s xuống 2 s
mới là thay đổi thật. Luôn lập bảng này trước khi chọn kỹ thuật tối ưu — nó nói cho biết nên
tối ưu chỗ nào.

## Những thứ hay bị quên trong ngân sách

| Khoản | Ảnh hưởng |
|---|---|
| Thời gian bootloader + khởi tạo IDF sau mỗi lần deep sleep | 200–500 ms ở dòng khá cao; chu kỳ ngắn (< 10 s) thì deep sleep có khi **tốn hơn** light sleep |
| Dòng nghỉ của LDO/DC-DC | 5 µA (LDO tốt) tới 5 mA (AMS1117) — có thể lớn gấp trăm lần dòng ngủ của chip |
| Tự xả của pin | Li-ion 2–3%/tháng, alkaline thấp hơn, LiSOCl2 rất thấp. Với thiết kế nhiều năm, đây có thể là khoản lớn nhất. |
| Nhiệt độ thấp | Dung lượng pin giảm mạnh dưới 0 °C; Li-ion mất 20–40% ở −10 °C |
| Dòng đỉnh khi TX | 300–500 mA xung; pin cúc áo và pin cũ không cấp nổi → sụt áp → brownout. Cần tụ đệm. |
| Retry khi mạng kém | Mỗi lần kết nối thất bại vẫn tốn gần đủ năng lượng như thành công. Ngân sách phải tính cho tỉ lệ thất bại thực tế, không phải cho trường hợp lý tưởng. |
| Ghi NVS/flash | Xung dòng vài chục mA và mất vài ms; ghi mỗi chu kỳ thì cộng lại đáng kể (và mòn flash) |

Khoản "retry khi mạng kém" hay bị bỏ qua nhất: thiết bị tính được 12 tháng trong phòng lab,
ra hiện trường sóng yếu, mỗi chu kỳ retry 3 lần → chỉ còn 4 tháng. Luôn đưa hệ số dự phòng
(ví dụ ×1.3–1.5) và nói rõ hệ số đó cho người dùng.

## Chọn pin — hệ quả tới firmware

| Loại | Điện áp | Ghi chú tới firmware |
|---|---|---|
| Li-ion / LiPo 1 cell | 3.0–4.2 V | Cần bảo vệ và theo dõi mức pin; dải áp rộng nên phải test ở 3.2 V, không chỉ ở 4.0 V |
| 2×AA alkaline | 1.8–3.2 V | Cần boost; hiệu suất boost thấp khi tải nhỏ — đo cả tổn hao của mạch boost |
| LiSOCl2 (ER14505) | 3.6 V | Tự xả cực thấp, hợp thiết kế nhiều năm, nhưng **dòng đỉnh kém** — bắt buộc tụ đệm cho pha TX |
| Pin cúc áo CR2032 | 3.0 V | Dòng đỉnh rất hạn chế; Wi-Fi hầu như không khả thi, cân nhắc BLE hoặc ESP-NOW |

Đo mức pin: dùng chia áp có MOSFET cắt (chia áp thường trực cũng rò dòng liên tục), đo bằng ADC
sau khi hệ đã ổn định, và **không đo ngay trong lúc TX** — số đọc sẽ tụt do sụt áp tức thời chứ
không phải do hết pin. Hiệu chuẩn ADC → `esp32-04-driver-development`.

## Đo cho đúng

- Công cụ: power profiler (Nordic PPK2, Otii Arc) hoặc shunt + oscilloscope có băng thông đủ.
- Đồng hồ vạn năng **không dùng được** để kết luận: dải tự động của nó không bắt kịp chuyển từ
  µA sang hàng trăm mA, và burden voltage ở thang µA có thể làm chip reset.
- Đo trên **board sản phẩm tối giản**, không đo trên DevKit. DevKit có LED nguồn, chip USB-UART
  và LDO dòng nghỉ cao — kết quả sai hoàn toàn (xem `peripheral-power.md`).
- Đo **một chu kỳ đầy đủ**, chia theo pha, chứ không chỉ đo dòng ngủ. Con số dòng ngủ đẹp mà pha
  radio dài thì tuổi thọ vẫn tệ.
- Lặp lại phép đo ở điều kiện xấu: sóng yếu, server không phản hồi, pin gần cạn. Đây mới là
  điều kiện quyết định tuổi thọ thật.

## Ước lượng trong firmware

Thiết bị nên tự ước tính năng lượng đã dùng và gửi lên cùng telemetry:

```c
typedef struct {
    uint32_t cycles;              /* số chu kỳ từ lần cấp nguồn */
    uint32_t active_ms_total;     /* tổng thời gian không ngủ */
    uint32_t radio_ms_total;      /* tổng thời gian radio bật — khoản tốn nhất */
    uint32_t connect_failures;    /* mỗi lần thất bại vẫn đốt năng lượng */
    uint16_t vbat_mv;
} pm_stats_t;
```

`radio_ms_total` và `connect_failures` cho biết vì sao pin tụt nhanh hơn dự tính mà không cần
mang thiết bị về. Không có hai con số này thì mọi chẩn đoán ngoài hiện trường đều là đoán.
