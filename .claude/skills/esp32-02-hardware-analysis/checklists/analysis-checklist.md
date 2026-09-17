# Checklist thẩm định phần cứng — 10 hạng mục

Mỗi mục ghi **PASS / FAIL / UNKNOWN** kèm nguồn bằng chứng.
UNKNOWN phải chuyển thành một câu hỏi cụ thể trong mục "Thiếu thông tin", **không được làm tròn
thành PASS**.

## 0. Tiền đề — chốt trước khi kiểm tra
- [ ] Tên module chính xác (không chỉ "ESP32"): ví dụ ESP32-S3-WROOM-1-N16R8.
      Hậu tố N/R quyết định dung lượng flash và có PSRAM hay không.
- [ ] Board: devkit thương mại (tên + revision) hay mạch custom (cần schematic).
- [ ] Danh sách ngoại vi cần dùng, kèm số lượng từng loại.
- [ ] Pin map hiện có (nếu đang thẩm định dự án sẵn).

Thiếu mục 0 thì **dừng, hỏi** — mọi kết luận sau đó đều vô nghĩa.

## 1. GPIO conflict
- [ ] Không có chân nào được gán cho hai chức năng.
- [ ] Không dùng chân board đã chiếm (LED onboard, nút BOOT, cảm biến tích hợp, khe SD).
- [ ] Chân số có tồn tại trên gói module này (module không đưa ra hết chân của die).
- [ ] Chân có được nối ra header không, hay chỉ tồn tại trên die.

## 2. Boot / strapping restriction
- [ ] Không có strapping pin nào bị mạch ngoài kéo về mức sai lúc reset.
- [ ] Chân strapping nếu buộc phải dùng: chỉ dùng làm OUTPUT, và mạch ngoài không kéo ngược.
- [ ] ESP32 GPIO12 (MTDI): kéo cao lúc boot đặt điện áp flash về 1.8V → module không khởi động.
- [ ] LED nối trực tiếp vào strapping pin qua trở kéo lên là lỗi phổ biến — kiểm tra.
- [ ] Nút nhấn trên strapping pin phải là pull lên/xuống đúng chiều mặc định.

## 3. Peripheral conflict
- [ ] Số instance yêu cầu không vượt số instance chip có (UART, I2C, SPI, LEDC, MCPWM, TWAI, I2S, RMT).
- [ ] SPI: không dùng host gắn với flash nội.
- [ ] LEDC: các kênh dùng chung timer phải cùng tần số và độ phân giải.
- [ ] Không hai ngoại vi cùng đòi một timer group / kênh DMA.
- [ ] Chân tốc độ cao (SPI, I2S) nên nằm trên IOMUX; qua GPIO matrix sẽ bị giới hạn tần số.
- [ ] TWAI/CAN cần transceiver ngoài (SN65HVD230 hoặc tương đương) — chip không tự lái bus.

## 4. Voltage compatibility
- [ ] Mọi tín hiệu vào chân ESP32 ở mức 3.3V. Thiết bị 5V phải qua level shifter.
- [ ] Chân đầu ra 3.3V có đủ ngưỡng VIH cho thiết bị nhận 5V không (nhiều thiết bị cần trên 3.5V).
- [ ] Nguồn cấp cho module đúng dải (3.0–3.6V), không cấp 5V vào chân 3V3.
- [ ] Điện áp flash (1.8V hay 3.3V) đúng với module đang dùng.
- [ ] Chân analog không bị đưa điện áp vượt Vref.

## 5. Input/output capability
- [ ] Chân input-only không được dùng làm output (ESP32 GPIO34–39).
- [ ] Chân cần open-drain (I2C, 1-Wire) có cấu hình OD được không.
- [ ] Dòng ra mỗi chân không vượt giới hạn — LED phải có trở hạn dòng, không lái tải trực tiếp.
- [ ] Tổng dòng qua các chân không vượt giới hạn toàn chip.
- [ ] Tải cần dòng lớn phải qua transistor/MOSFET/driver, không lấy trực tiếp từ GPIO.

## 6. Pull-up / pull-down
- [ ] Chân input-only không có pull nội (ESP32 GPIO34–39) → bắt buộc pull ngoài.
- [ ] I2C: có pull-up ngoài 2.2k–10k lên 3.3V. Pull nội quá yếu cho 400kHz.
- [ ] Nút nhấn: có pull xác định, không để chân float.
- [ ] Chân điều khiển cơ cấu chấp hành có pull-down ngoài để giữ trạng thái an toàn lúc reset
      (lúc reset chân ESP32 float, tải có thể tự bật trong vài chục ms đầu).
- [ ] Pull nội không hoạt động trong deep sleep trừ khi cấu hình qua RTC domain.

## 7. Interrupt capability
- [ ] Chân cần ngắt có hỗ trợ ngắt GPIO không.
- [ ] Số nguồn ngắt không vượt giới hạn; nhiều chân có thể dùng chung một ISR.
- [ ] Chân cần đánh thức từ deep sleep phải là **RTC GPIO** (C3/C6 dùng cơ chế GPIO wake riêng).
      Chọn chân không phải RTC là lỗi hay gặp nhất — biểu hiện: thiết bị ngủ mãi không dậy.
- [ ] Tín hiệu ngắt tần số cao có cần bộ lọc glitch / debounce phần cứng không.

## 8. Boot / debug / USB conflict
- [ ] UART0 dùng làm console log — không gán cho thiết bị ngoài nếu còn cần log.
- [ ] Chân USB D+/D− (nếu dùng USB-OTG hoặc USB-Serial-JTAG) không bị gán việc khác.
- [ ] Chân JTAG không bị chiếm nếu còn cần gỡ lỗi bằng JTAG.
- [ ] Nút BOOT/EN hoạt động đúng, vào được download mode để nạp firmware.
- [ ] Mạch auto-reset (DTR/RTS) không bị mạch ngoài can thiệp.

## 9. Flash / PSRAM restriction
- [ ] Không dùng chân nối SPI flash nội bộ (ESP32 GPIO6–11).
- [ ] Module có PSRAM octal (hậu tố R8): các chân bị PSRAM chiếm không được dùng (S3 GPIO33–37).
- [ ] Dung lượng flash khai báo trong sdkconfig khớp flash thật trên module.
- [ ] Chế độ flash (QIO/DIO) và tần số phù hợp module — sai gây boot lỗi ngẫu nhiên.

## 10. Power risk
- [ ] Nguồn chịu được dòng đỉnh khi Wi-Fi TX (có thể vọt lên khoảng 500 mA trong thời gian ngắn).
- [ ] Có tụ decoupling gần chân nguồn module (thường 100nF + 10µF trở lên).
- [ ] LDO/nguồn có đủ headroom, không nóng quá khi tải đầy.
- [ ] Tải cảm ứng (relay, motor, solenoid) có diode flyback hoặc snubber.
- [ ] Tải công suất có nguồn riêng, chung GND, không lấy từ chân 3V3 của devkit.
- [ ] Inrush khi bật tải không kéo sụt nguồn gây brownout reset.
- [ ] Cơ cấu chấp hành ở trạng thái AN TOÀN trong toàn bộ thời gian boot, không chỉ sau khi
      firmware chạy.
- [ ] Chạy pin: tính đến dòng rò của mạch ngoài (LED nguồn, chip USB-UART, LDO dòng nghỉ).

## Khi có UNKNOWN — mẫu câu hỏi

Hỏi cụ thể, nêu rõ cần tài liệu gì và tìm ở đâu. Không hỏi chung chung:

- "Board có pull-up trên SDA/SCL không? Cần schematic, hoặc đo điện trở giữa SDA và 3V3
  khi mất nguồn."
- "Module là ESP32-S3-WROOM-1 bản N8 hay N16R8? In trên nhãn kim loại của module.
  Có R8 nghĩa là có PSRAM octal và GPIO33–37 bị chiếm."
- "Cảm biến X mức logic bao nhiêu? Cần trang Electrical Characteristics của datasheet."
- "GPIO4 hiện nối gì trên board? Cần schematic — đây là strapping pin trên C6."
