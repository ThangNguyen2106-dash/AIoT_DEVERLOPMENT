# Điện áp, dòng điện và rủi ro nguồn

Phục vụ hạng mục #4 và #10 của checklist.

## Mức logic

ESP32 là thiết bị **3.3V**. Không có chân nào chịu 5V.

| Tình huống | Xử lý |
|---|---|
| Thiết bị 5V → chân ESP32 | **bắt buộc** level shifter hoặc cầu chia áp (chỉ cho tín hiệu chậm) |
| ESP32 → thiết bị 5V | kiểm tra VIH của thiết bị nhận. Nhiều thiết bị cần trên 3.5V → 3.3V không đủ |
| Bus hai chiều 5V (I2C) | level shifter MOSFET hai chiều (BSS138), không dùng cầu chia áp |
| UART 5V | level shifter, hoặc chọn module bản 3.3V |
| RS485 / CAN transceiver | chọn bản 3.3V, hoặc shift chân RX |

Cầu chia áp chỉ chấp nhận được cho tín hiệu một chiều, tốc độ thấp. Với I2C, SPI tốc độ cao,
cầu chia áp làm méo cạnh và gây lỗi khó tìm.

Đưa 5V vào chân ESP32 làm hỏng chân hoặc cả chip — mức **CHẶN** không thương lượng.

## Nguồn cấp

- Module cần nguồn trong dải khoảng 3.0–3.6V, ổn định.
- Cấp 5V vào chân 3V3 làm hỏng module ngay.
- Tụ decoupling gần chân nguồn: thường 100nF + 10µF trở lên. Thiếu tụ gây reset ngẫu nhiên
  khi Wi-Fi phát.
- LDO phải đủ headroom và đủ tản nhiệt ở tải đầy. AMS1117 từ 5V xuống 3.3V ở 500mA toả nhiệt
  đáng kể.

## Dòng điện

| Trạng thái | Dòng tham khảo |
|---|---|
| Wi-Fi TX đỉnh | có thể vọt lên khoảng 500 mA trong thời gian ngắn |
| Wi-Fi hoạt động bình thường | vài chục đến hơn 100 mA |
| Không radio | vài chục mA |
| Deep sleep | vài chục µA (phụ thuộc mạch ngoài nhiều hơn phụ thuộc chip) |

Số liệu thay đổi theo dòng chip và module — dùng làm ước lượng thiết kế, **không hứa con số
cụ thể**; đo thực tế thuộc `esp32-11-power-management`.

Hệ quả thiết kế:
- Nguồn phải chịu được dòng **đỉnh**, không chỉ dòng trung bình.
- Cổng USB yếu hoặc cáp kém là nguyên nhân brownout reset rất phổ biến. Triệu chứng:
  reset khi bắt đầu kết nối Wi-Fi.
- Pin CR2032 không cấp nổi dòng đỉnh Wi-Fi nếu không có tụ đệm lớn.

## Brownout

`Brownout detector was triggered` là **triệu chứng nguồn yếu**, không phải bug phần mềm.
Không đề xuất tắt brownout detector để "hết lỗi" — làm vậy chỉ chuyển lỗi thành hỏng dữ liệu
flash và reset không đoán trước.

Hướng xử lý đúng: nguồn khoẻ hơn, cáp tốt hơn, thêm tụ đệm, giảm dòng đỉnh (ramp tải,
không bật nhiều tải cùng lúc).

## Tải ngoài

- **Không lấy nguồn cho tải công suất từ chân 3V3 của devkit.** LDO trên devkit thường chỉ
  vài trăm mA và còn phải nuôi chính module.
- Tải công suất dùng nguồn riêng, **chung GND** với ESP32.
- Tải cảm ứng (relay, motor, solenoid, van điện từ) bắt buộc có diode flyback hoặc snubber.
  Thiếu là hỏng transistor lái và gây reset do xung ngược.
- Inrush khi bật tải (motor khởi động, đèn, tụ lớn) kéo sụt nguồn — cần ramp hoặc mạch soft-start.
- LED công suất, motor, relay không lái trực tiếp từ GPIO; dùng transistor/MOSFET/driver
  có trở base/gate đúng.

## An toàn trạng thái boot

Trong toàn bộ thời gian từ khi cấp nguồn đến khi firmware chạy (reset, bootloader, khởi tạo),
chân GPIO ở trạng thái float. Nếu mạch ngoài không định nghĩa mức an toàn:

- relay có thể đóng
- motor có thể quay
- heater có thể bật

Đây là rủi ro an toàn thật. Yêu cầu bắt buộc: **pull-down (hoặc pull-up, tuỳ mạch lái) bên ngoài**
đặt tải ở trạng thái không hoạt động, độc lập với firmware. Không dựa vào code để bảo đảm
an toàn lúc boot — code chưa chạy.

Với cơ cấu chấp hành có thể gây thương tích hoặc hư hỏng, nêu ở mức **CHẶN** nếu thiết kế
không có bảo vệ phần cứng độc lập.

## Chạy pin

Ghi nhận ở đây, phân tích sâu thuộc `esp32-11-power-management`:
- Dòng rò của mạch ngoài (LED nguồn, chip USB-UART, LDO dòng nghỉ) thường lớn hơn dòng ngủ
  của chip nhiều lần.
- Cảm biến ngoài phải tắt được (MOSFET cấp nguồn, hoặc dùng chế độ sleep của cảm biến).
- Điện trở pull luôn dẫn dòng khi chân ở mức đối nghịch — chọn giá trị lớn cho thiết kế pin.

## ESD và bảo vệ

Với sản phẩm thương mại, nêu ở mức CẢNH BÁO nếu thiếu:
- Bảo vệ ESD ở các chân ra ngoài vỏ (USB, đầu nối, nút bấm).
- Bảo vệ ngược cực nguồn.
- Cách ly cho tín hiệu đi dây dài hoặc ra môi trường nhiễu (opto, transceiver cách ly).
