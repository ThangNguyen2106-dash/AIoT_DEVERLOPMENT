# Checklist rà soát thiết kế năng lượng

Mỗi mục **PASS / FAIL / KHÔNG ÁP DỤNG**. Còn FAIL thì chưa được coi là xong, và **không được
báo cáo con số tuổi thọ pin** như thể đã chốt.

## 0. Tiền đề — chốt trước khi tối ưu
- [ ] Thiết bị thật sự chạy pin hoặc có ràng buộc công suất. (Cắm điện ⇒ dừng, không tối ưu điện.)
- [ ] Dung lượng pin **hiệu dụng** (mAh), không phải con số in trên vỏ.
- [ ] Tuổi thọ mục tiêu.
- [ ] Chu kỳ đo và chu kỳ gửi (hai thứ khác nhau).
- [ ] Độ trễ chấp nhận được: với sự kiện tại chỗ, và với lệnh từ xa.
- [ ] Dữ liệu được phép mất bao nhiêu.
- [ ] Thiết bị có cơ cấu chấp hành không; trạng thái an toàn là gì.

Thiếu mục 0 thì dừng và hỏi — mọi tối ưu sau đó là mò.

## 1. Ngân sách
- [ ] Có bảng mAs theo pha cho **một chu kỳ đầy đủ**, không chỉ có dòng ngủ.
- [ ] Đã tính thời gian boot + init sau mỗi lần deep sleep.
- [ ] Đã tính dòng nghỉ của LDO/DC-DC và mọi mạch ngoài.
- [ ] Đã tính tự xả của pin (quan trọng với thiết kế nhiều năm).
- [ ] Đã tính chi phí **retry khi mạng kém**, không chỉ trường hợp lý tưởng.
- [ ] Có hệ số dự phòng và hệ số đó được nói rõ cho người dùng.
- [ ] Đã kiểm tra dòng đỉnh TX: pin và tụ đệm cấp nổi (tránh brownout).

## 2. Đo đạc
- [ ] Đo bằng power profiler hoặc shunt + oscilloscope, **không** bằng đồng hồ vạn năng.
- [ ] Đo trên board sản phẩm tối giản, không đo trên DevKit.
- [ ] Đã đo dòng nền với firmware "ngủ ngay" để tách phần cứng khỏi firmware.
- [ ] Đã đo ở điều kiện xấu: sóng yếu, server không phản hồi, pin gần cạn.
- [ ] Số liệu trước/sau mỗi thay đổi đều có, không chỉ ước lượng.

## 3. Chọn chế độ
- [ ] Chế độ của từng pha được chọn theo yêu cầu latency ở mục 0, không theo "cái nào rẻ nhất".
- [ ] Với chu kỳ ngắn: đã kiểm tra deep sleep có thật sự rẻ hơn light sleep không (ngưỡng hoà vốn).
- [ ] Nếu bật `CONFIG_PM_ENABLE`: mọi task đều block thật (kiểm bằng run-time stats, IDLE cao).
- [ ] Driver nhạy timing (bit-bang, RMT, I2S, UART tốc độ cao) đã giữ power lock.
- [ ] Nếu hạ tần số CPU: đã đo được lợi ích thật, không phải giả định.

## 4. Wake source
- [ ] Mọi chân wake là RTC/LP GPIO của **đúng target** (tra datasheet).
- [ ] Có ít nhất một nguồn đánh thức không phụ thuộc bên ngoài (timer) — không thể ngủ vĩnh viễn.
- [ ] `esp_sleep_get_wakeup_cause()` được xử lý đầy đủ, gồm cả `UNDEFINED`.
- [ ] Chân wake có mức xác định bằng pull **ngoài** (pull nội không sống qua deep sleep).
- [ ] Có chống thức liên tục (nút kẹt, cảm biến kích liên tục) và có báo cáo tình trạng đó.

## 5. Giữ trạng thái
- [ ] Dữ liệu RTC memory có magic + CRC và được kiểm tra trước khi dùng.
- [ ] `UNDEFINED` wake cause ⇒ coi RTC memory là không hợp lệ.
- [ ] NVS **không** bị ghi mỗi chu kỳ (mòn flash + tốn điện).
- [ ] Đã quyết rõ: cái gì vào RTC mem, cái gì vào NVS, cái gì chấp nhận mất.
- [ ] Buffer dữ liệu chưa gửi: đã nói rõ mất bao nhiêu nếu mất nguồn, và người dùng chấp nhận.
- [ ] Timestamp: đã xử lý trôi RTC và đánh dấu dữ liệu có timestamp không tin cậy.
- [ ] Ghim BSSID/IP có **fallback** quét/DHCP đầy đủ khi thất bại, và có đếm số lần fallback.

## 6. Ngoại vi
- [ ] Đã chạy hết danh sách truy dòng rò (LED, USB-UART, LDO, cảm biến, chia áp, pull, chân float).
- [ ] Trước khi cắt nguồn module: mọi chân tín hiệu tới nó đã về LOW/hi-Z (chống cấp nguồn ký sinh).
- [ ] `t_startup` của cảm biến lấy từ datasheet, không đoán.
- [ ] Cảm biến cần làm nóng / có bộ lọc nội: đã kiểm tra cắt nguồn không làm **sai số liệu**.
- [ ] Chân không dùng đã `rtc_gpio_isolate()`.
- [ ] Đo pin không thực hiện trong lúc TX.
- [ ] Radio bật muộn nhất, tắt sớm nhất; đo cảm biến xong mới bật radio.

## 7. An toàn và vận hành
- [ ] Không có nơi nào trong ứng dụng gọi thẳng `esp_deep_sleep_start()` ngoài module pm.
- [ ] Có khoá chống ngủ, và nó được giữ khi: đang ghi flash, đang OTA, đang gửi dở,
      cơ cấu chấp hành đang bật.
- [ ] Cơ cấu chấp hành ở trạng thái an toàn trước khi ngủ (hoặc giữ khoá không cho ngủ).
- [ ] Có **chế độ bảo dưỡng / cửa sổ OTA** — thiết bị ngoài hiện trường vẫn cập nhật được.
- [ ] Có ngân sách thời gian tối đa cho pha radio; hết thì bỏ cuộc, buffer, ngủ, backoff.
- [ ] Watchdog và brownout detector **vẫn bật**.
- [ ] Telemetry chẩn đoán tối thiểu còn giữ: lý do reset, `connect_failures`, `radio_ms_total`,
      vbat, số chu kỳ.

## 8. Công bố đánh đổi (bắt buộc)
- [ ] Mỗi thay đổi có một dòng trong bảng **Power / Latency / Reliability / Functionality**.
- [ ] Độ trễ nhận lệnh từ xa đã được nói bằng **con số**, không phải "hơi chậm".
- [ ] Lượng dữ liệu có thể mất đã được nói bằng con số.
- [ ] Những tối ưu đã cân nhắc nhưng **từ chối** cũng được liệt kê kèm lý do.
- [ ] Mọi đánh đổi thuộc nhóm "cần xác nhận" (`tradeoffs.md`) đã được người dùng xác nhận rõ ràng.
- [ ] Nếu không đạt mục tiêu: đã báo trung thực con số đạt được + danh sách thứ phải hy sinh để
      đạt mục tiêu, thay vì âm thầm cắt chức năng cho vừa con số.
