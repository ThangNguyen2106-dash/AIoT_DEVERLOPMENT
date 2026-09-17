# Tầng 3 — Hardware validation

Cái chỉ chứng minh được bằng phần cứng thật: timing, mức điện, nhiễu, độ bền theo thời gian.
Đắt nhất, chậm nhất, và **không có gì thay thế được**.

## Peripheral test

Mỗi ngoại vi trong pin map phải có một lần kiểm chứng trên board thật. Ghi lại thành bảng.

| Ngoại vi | Kiểm gì | Bằng gì | Đạt khi |
|---|---|---|---|
| GPIO out | mức cao/thấp thật, drive đủ tải | đồng hồ / oscilloscope | đúng mức, không sụt khi có tải |
| GPIO in | đọc đúng, có pull đúng, chống dội | tín hiệu thật + scope | không đọc nhầm khi nhiễu |
| I2C | ACK từ đúng địa chỉ, dạng sóng, pull-up đủ | scope / logic analyzer | cạnh lên đủ nhanh ở tốc độ đang dùng |
| SPI | mode (CPOL/CPHA) đúng, tốc độ tối đa ổn định | logic analyzer | dữ liệu đúng ở tốc độ đặt, không lỗi bit |
| UART | baud thật, lỗi khung, chịu được luồng liên tục | scope + truyền dài | 0 lỗi khung qua N KB |
| ADC | đường chuẩn với nguồn áp đã biết, nhiễu | nguồn chuẩn / DMM | sai số trong ngưỡng, nhiễu chấp nhận được |
| PWM/LEDC | tần số và duty thật | scope | lệch trong dung sai |
| Timer/ISR | chu kỳ và jitter | GPIO toggle + scope | jitter dưới ngưỡng thiết kế |
| I2S/RMT/TWAI | dạng sóng và tính toàn vẹn khung | analyzer chuyên dụng | đúng chuẩn giao thức |

Pin map và ràng buộc điện → `esp32-02-hardware-analysis`.
Code cấu hình ngoại vi → `esp32-04-driver-development`.

## Communication test (đầu-cuối, thiết bị thật)

- Kết nối lần đầu từ trạng thái trắng (NVS rỗng): mất bao lâu, có thành công không.
- Vùng sóng yếu: RSSI thấp → còn kết nối được không, throughput còn bao nhiêu.
- Rớt và phục hồi: tắt AP N phút rồi bật lại; đo thời gian phục hồi, kiểm dữ liệu
  trong thời gian offline có được gửi bù không, có trùng lặp không.
- Nhiều thiết bị cùng lúc (nếu sản phẩm chạy nhiều con): có va chạm, có sập server không.
- Chuyển vùng/đổi AP, đổi kênh, mất DNS.
- Đo mức tiêu thụ trong lúc truyền nếu chạy pin → `esp32-11-power-management`.

## Điện và timing

- Dòng đỉnh lúc bật radio và lúc cơ cấu chấp hành đóng — nguồn có chịu được không.
- Trạng thái chân **trong lúc boot** (trước khi firmware chạy): relay có bật nhầm không.
  Đây là lỗi hay gặp và nguy hiểm.
- Thời gian từ cấp nguồn tới lúc firmware sẵn sàng.
- Độ trễ đường quan trọng nhất (sự kiện → phản ứng), đo bằng GPIO toggle + scope.

## Soak test

Chạy liên tục nhiều ngày trước khi ship. Đây là thứ duy nhất bắt được lỗi tích luỹ.

Theo dõi, ghi định kỳ (ví dụ mỗi 5 phút) vào log:
- Free heap và **minimum free heap từ khi boot** — xu hướng giảm đều = rò rỉ.
- Mảnh vụn heap: khối liên tục lớn nhất (`heap_caps_get_largest_free_block`).
- Stack high-water mark từng task.
- Số lần reconnect, số lỗi từng loại, số reset và reset reason.
- Uptime — mọi lần về 0 đều phải giải thích được.

Tiêu chí đạt:
- Không reset ngoài kế hoạch trong suốt thời gian chạy.
- Heap tối thiểu ổn định (dao động quanh một mức, không trôi xuống).
- Khối liên tục lớn nhất không co dần (phân mảnh).
- Bộ đếm không tràn/không nhảy âm; mốc thời gian không sai sau nhiều ngày
  (tràn `uint32_t` mili giây ở ~49.7 ngày — test riêng mục này bằng cách đặt sẵn bộ đếm).

Thời lượng tối thiểu: 24 giờ cho medium, 7 ngày trở lên cho production, và phải bao
được ít nhất một chu kỳ dài nhất của sản phẩm (báo cáo ngày, xoay log, gia hạn token).

Phân tích rò rỉ và phân mảnh → `esp32-09-performance-optimization`.

## Hardware-in-the-loop (HIL)

Nếu có ngân sách: một board cắm cố định vào máy CI, có thể nạp firmware và reset tự động.

Tối thiểu cần: nạp qua USB tự động, đọc log UART, cắt/cấp nguồn bằng relay điều khiển
được, và vài chân loopback. Có thêm nguồn lập trình được và logic analyzer thì chạy được
cả fault injection phần nguồn.

Chạy nightly: nạp bản mới nhất → chạy bộ Unity target test → chạy vài kịch bản fault
injection → soak ngắn → báo cáo. Đây là cách duy nhất giữ tầng 3 khỏi bị bỏ quên.

## Ghi chép

Tầng này không lặp lại rẻ, nên **kết quả phải được ghi lại**, kèm: ngày, phiên bản
firmware, board/serial nào, thiết bị đo gì, số đo thật (không phải "OK"). Ảnh scope
hoặc số liệu cụ thể. Lần sau đổi code chỉ cần chạy lại phần bị ảnh hưởng.
