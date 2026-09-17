# Checklist trước khi nạp firmware (bắt buộc rà soát)

## Phần mềm
- [ ] Build sạch, không warning mới.
- [ ] `idf.py size` — flash và RAM còn dư so với partition đang dùng.
- [ ] Đúng chip target (`idf.py set-target`) khớp board thật.
- [ ] Partition table đủ chỗ cho binary; nếu có OTA thì cả 2 slot đều đủ.

## Phần cứng — hỏi người dùng nếu chưa rõ
- [ ] Cổng serial đúng thiết bị (không nạp nhầm board khác đang cắm).
- [ ] Nguồn cấp đủ dòng — Wi-Fi bật có thể vọt lên ~500 mA, USB port yếu sẽ gây brownout reset.
- [ ] Mọi tín hiệu vào chân ESP32 đều ở mức 3.3V. Cảm biến/module 5V phải qua level shifter.
- [ ] Không có chân strapping bị kéo sai (xem `esp32-02-hardware-analysis/references/chip-matrix.md`).
- [ ] Tải công suất (motor, relay, LED strip) có nguồn riêng và chung GND, không lấy từ chân 3V3.
- [ ] Nếu firmware điều khiển cơ cấu chấp hành: trạng thái mặc định lúc boot là AN TOÀN
      (motor dừng, relay mở, heater tắt) trước khi logic chạy.

## Hành động KHÔNG BAO GIỜ tự chạy khi chưa có xác nhận rõ ràng của người dùng
- `espefuse.py burn_efuse` / `burn_key` — KHÔNG THỂ HOÀN TÁC, sai là hỏng chip vĩnh viễn.
- Bật Secure Boot hoặc Flash Encryption ở chế độ Release.
- `esptool.py erase_flash` — xoá cả NVS, mất hiệu chuẩn/khoá/cấu hình thiết bị.
- Đổi tần số/điện áp flash, đổi partition table trên thiết bị đã triển khai ngoài thực địa.
