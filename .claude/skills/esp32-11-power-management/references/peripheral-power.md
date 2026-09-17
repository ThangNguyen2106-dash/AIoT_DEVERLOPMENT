# Nguồn ngoại vi, dòng rò và duty cycle cảm biến

Phần lớn ca "ESP32 ngủ mà vẫn ăn mA" nằm ở đây, **không phải trong firmware**. Chạy hết danh sách
truy nguyên bên dưới trước khi sửa một dòng code nào.

## Truy nguyên dòng rò — theo thứ tự

| # | Nghi phạm | Dòng điển hình | Cách xác minh / xử lý |
|---|---|---|---|
| 1 | LED nguồn trên DevKit | 1–5 mA | tháo LED hoặc đo trên board sản phẩm; **lớn hơn cả chip lúc ngủ** |
| 2 | Chip USB-UART (CP2102, CH340, FT232) | 1–10 mA | không có trên board sản phẩm; nếu có, phải cắt nguồn được |
| 3 | LDO dòng nghỉ cao (AMS1117 ~5 mA) | tới 5 mA | đổi sang LDO µA (HT7333, XC6206, TPS7A02) hoặc DC-DC có chế độ PFM |
| 4 | Cảm biến/module ngoài không tắt | 0.1–50 mA | cắt nguồn bằng MOSFET, hoặc dùng sleep mode của chính cảm biến |
| 5 | Chia áp đo pin thường trực | 3.3V/(R1+R2) | thêm MOSFET cắt, hoặc dùng trở rất lớn (đánh đổi: nhiễu, thời gian ổn định) |
| 6 | Pull-up/pull-down dẫn dòng | V/R mỗi chân | tính từng cái; I2C 4.7k ở mức thấp = 0.7 mA |
| 7 | Chân float lúc ngủ | vài trăm µA, thất thường | `rtc_gpio_isolate()` cho chân không dùng |
| 8 | Chân output vẫn lái tải khi ngủ | tuỳ tải | đưa về mức an toàn + `gpio_hold_en` trước khi ngủ |
| 9 | Dòng ngược qua chân IO vào module đã tắt nguồn | vài trăm µA | chân nối tới module đã cắt nguồn phải về LOW/hi-Z trước khi cắt |
| 10 | Điện trở kéo trên đường I2C tới thiết bị đã tắt nguồn | mA | cắt cả pull-up theo cùng đường nguồn của thiết bị |

Mục 9 là bẫy tinh vi và rất hay gặp: cắt nguồn cảm biến nhưng SDA/SCL (hoặc CS/MOSI) vẫn ở mức
cao → dòng chảy ngược qua diode bảo vệ của cảm biến, cấp nguồn ký sinh cho nó. Kết quả: cắt nguồn
mà không tiết kiệm được gì, đôi khi còn làm cảm biến ở trạng thái nửa vời không khởi động lại
được. **Trước khi cắt nguồn: đưa mọi chân tín hiệu tới thiết bị đó về LOW hoặc hi-Z.**

## Cô lập chân khi ngủ

```c
/* chân không dùng, hoặc chân có pull ngoài mà không cần trong lúc ngủ */
rtc_gpio_isolate(GPIO_NUM_12);

/* chân điều khiển tải: đưa về trạng thái an toàn rồi giữ */
gpio_set_level(VALVE_GPIO, 0);
gpio_hold_en(VALVE_GPIO);
gpio_deep_sleep_hold_en();
```

`rtc_gpio_isolate()` chỉ áp dụng cho RTC GPIO. Chân thường không cô lập được — nếu chân đó rò thì
phải xử lý bằng phần cứng.

Pull nội **không sống qua deep sleep** trừ khi giữ RTC peripheral domain (tốn thêm dòng). Thiết kế
đúng là dùng pull **ngoài** cho mọi chân cần mức xác định khi ngủ. Ràng buộc chân →
`esp32-02-hardware-analysis/references/pin-constraints.md`.

## Cắt nguồn ngoại vi bằng MOSFET

Mạch cắt nguồn (high-side P-MOSFET cho tải 3.3V, hoặc load switch tích hợp) là thiết kế phần cứng
→ `esp32-02-hardware-analysis`. Về phía firmware cần giữ đúng trình tự:

```
BẬT:  bật nguồn → chờ t_startup của cảm biến → init bus → đọc
TẮT:  kết thúc giao dịch → đưa chân tín hiệu về LOW/hi-Z → tắt nguồn → (tuỳ) isolate chân
```

`t_startup` lấy từ datasheet cảm biến, không đoán. Bỏ qua nó gây lỗi đọc rải rác rất khó truy —
và "sửa" bằng retry thì vừa sai vừa tốn điện.

Đánh đổi của việc cắt nguồn cảm biến, phải công bố:

| Trục | Ảnh hưởng |
|---|---|
| Power | tiết kiệm toàn bộ dòng nghỉ của cảm biến |
| Latency | thêm `t_startup` mỗi lần đo (có cảm biến cần hàng trăm ms, cảm biến khí cần hàng phút) |
| Reliability | mỗi lần khởi động lại là một cơ hội lỗi init; cần xử lý init failure |
| Functionality | mất dữ liệu liên tục; mất khả năng phát hiện sự kiện giữa hai lần đo; mất bộ lọc/trung bình nội của cảm biến |

Với cảm biến có bộ lọc nội hoặc cần làm nóng (cảm biến khí MOX, một số cảm biến bụi), cắt nguồn
mỗi chu kỳ làm **dữ liệu sai** chứ không chỉ chậm. Đó là mất chức năng, không phải tối ưu — phải
nói rõ.

## Duty cycle cảm biến

Ba mức, chọn theo yêu cầu chứ không theo mức tiết kiệm nhất:

| Mức | Cách làm | Khi nào dùng |
|---|---|---|
| Cắt nguồn hoàn toàn | MOSFET | cảm biến khởi động nhanh, dòng nghỉ cao, đo thưa |
| Sleep mode của cảm biến | lệnh qua I2C/SPI | cảm biến có chế độ ngủ tốt (nhiều cảm biến ngủ ở mức µA) |
| One-shot / single measurement | đặt chế độ đo một lần thay vì đo liên tục | đơn giản nhất, thường đủ |

Ưu tiên chế độ ngủ của chính cảm biến trước khi cắt nguồn: rẻ về phần cứng, giữ được cấu hình
và hiệu chuẩn nội, không có `t_startup` dài.

Trình tự đo tiết kiệm nhất cho một chu kỳ:

```
thức → bật nguồn nhóm cảm biến (một MOSFET cho cả nhóm nếu cùng yêu cầu)
     → chờ t_startup dài nhất trong nhóm
     → đọc tuần tự tất cả trong một lần bật
     → tắt
     → xử lý/nén dữ liệu khi ngoại vi đã tắt
     → chỉ bật radio khi đã có dữ liệu sẵn sàng gửi
```

Sai lầm hay gặp: bật Wi-Fi **trước** rồi mới đo cảm biến — radio bật suốt thời gian đo, tốn gấp
nhiều lần. Radio phải bật muộn nhất và tắt sớm nhất có thể.

## Đo pin

- Chia áp thường trực rò dòng liên tục: `3.3V / (100k+100k) = 16.5 µA` — bằng cả dòng ngủ của
  chip. Thêm MOSFET cắt, hoặc chấp nhận và **tính vào ngân sách** (đừng bỏ qua).
- Đo sau khi hệ ổn định, **không đo trong lúc TX** — sụt áp tức thời làm số đọc thấp giả, thiết
  bị sẽ báo hết pin sai.
- Với Li-ion, đường cong xả phẳng ở giữa dải: điện áp là chỉ báo thô. Muốn chính xác cần coulomb
  counter (phần cứng thêm).
- Hiệu chuẩn ADC và xử lý phi tuyến → `esp32-04-driver-development/references/adc.md`.

## Đo dòng cho đúng khi truy rò

1. Nạp firmware ngủ vĩnh viễn (`esp_deep_sleep_start()` ngay trong `app_main`) → đo dòng nền.
   Nếu đã cao ở đây thì **lỗi hoàn toàn ở phần cứng**, dừng sửa code.
2. Tháo dần từng ngoại vi / cắt từng nhánh nguồn, đo lại sau mỗi lần.
3. Chỉ khi dòng nền đạt mức datasheet mới quay lại kiểm tra firmware.

Đo trên board sản phẩm tối giản, cấp nguồn trực tiếp vào chân 3V3 (không qua USB). Công cụ và
sai lầm đo đạc → `power-budget.md`.
