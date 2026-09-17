# Flash và PSRAM

Phục vụ hạng mục #9 của checklist.

## Chân bị chiếm

| Trường hợp | Chân bị chiếm | Hậu quả nếu dùng |
|---|---|---|
| ESP32, flash quad ngoài | GPIO6–11 | chết boot ngay |
| ESP32-S3 + PSRAM octal (hậu tố R8) | GPIO33–37 | không boot hoặc PSRAM lỗi ngẫu nhiên |
| S2/S3 flash/PSRAM in-package | tuỳ biến thể, thường trong dải GPIO26–37 | **phải tra datasheet module** |
| C3/C6 | flash chiếm một số chân cố định | tra datasheet |

**Không suy đoán danh sách này từ số hiệu chân.** Biến thể module khác nhau chiếm chân khác
nhau. Thiếu tên module đầy đủ → hỏi, không đoán.

## Đọc hậu tố module

Ví dụ `ESP32-S3-WROOM-1-N16R8`:
- `N16` = 16 MB flash
- `R8` = 8 MB PSRAM **octal** → GPIO33–37 bị chiếm
- `R2`/`R8` khác nhau về quad hay octal tuỳ dòng — tra datasheet
- Không có hậu tố R = **không có PSRAM**

Hậu tố in trên nhãn kim loại của module. Nếu người dùng không đọc được, yêu cầu ảnh chụp nhãn
hoặc mã đặt hàng — đây là thông tin bắt buộc, không thay thế bằng phỏng đoán được.

## PSRAM — ràng buộc khi dùng

- **C3 và C6 không có PSRAM.** Đề xuất PSRAM cho hai dòng này là sai.
- PSRAM octal nhanh hơn quad nhưng chiếm nhiều chân hơn.
- Buffer **DMA thường không đặt được ở PSRAM** với nhiều ngoại vi. Kiểm tra từng ngoại vi
  trước khi chuyển buffer sang PSRAM; sai thì DMA đọc rác hoặc lỗi ngẫu nhiên.
- Bật PSRAM làm chậm một số truy cập bộ nhớ và tăng tiêu thụ điện.
- `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL` giữ cấp phát nhỏ ở RAM nội — cần cho hiệu năng.
- PSRAM và flash chia sẻ bus SPI; băng thông là tài nguyên chung.

## Flash — cấu hình phải khớp phần cứng

| Tham số | Rủi ro khi sai |
|---|---|
| Dung lượng (`CONFIG_ESPTOOLPY_FLASHSIZE`) | khai lớn hơn thật → partition vượt biên, mất dữ liệu hoặc không boot |
| Chế độ QIO/QOUT/DIO/DOUT | sai → boot lỗi ngẫu nhiên, hoặc không boot |
| Tần số (40/80 MHz) | quá cao với module hoặc layout kém → lỗi đọc chập chờn |
| Điện áp 1.8V / 3.3V | sai → không boot, có thể hỏng flash |

Triệu chứng flash cấu hình sai: boot được lúc nguội, lỗi khi nóng; hoặc reset ngẫu nhiên kèm
lỗi đọc flash. Dễ bị chẩn đoán nhầm thành bug phần mềm.

Kiểm tra flash thật trên thiết bị:
```
esptool.py --port <PORT> flash_id
```
Lệnh này chỉ đọc, an toàn. Đối chiếu kết quả với `CONFIG_ESPTOOLPY_FLASHSIZE` trong sdkconfig.

## Tuổi thọ flash

Flash NOR chịu khoảng 10k–100k chu kỳ xoá mỗi sector. Thiết kế ghi NVS/log thường xuyên phải
tính tuổi thọ: ghi mỗi giây sẽ làm hỏng vùng flash trong thời gian ngắn hơn vòng đời sản phẩm.
Chi tiết chiến lược ghi thuộc `esp32-06-application-development`; ở đây chỉ cần **nêu rủi ro**
nếu thấy yêu cầu ghi dày.

## Liên quan nhưng không thuộc skill này
- Thiết kế partition table, cấu hình sdkconfig → `esp32-01-project-init`
- Chiến lược ghi NVS, chống mòn flash → `esp32-06-application-development`
- Flash Encryption, eFuse VDD_SPI → `esp32-10-security`
