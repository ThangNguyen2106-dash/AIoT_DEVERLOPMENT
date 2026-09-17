# Sản xuất: nạp hàng loạt, danh tính thiết bị, test xuất xưởng

Nạp một board trên bàn và nạp 500 board ở xưởng là hai việc khác nhau. Khác biệt lớn nhất:
ở xưởng **không ai đọc log**, và một sai sót lặp lại 500 lần.

## Ba thứ mỗi thiết bị phải nhận được

| # | Thứ | Giống nhau toàn lô? |
|---|---|---|
| 1 | Firmware application | **có** — cùng một ảnh, cùng checksum |
| 2 | Danh tính thiết bị (serial, khoá, chứng chỉ) | **không** — duy nhất từng thiết bị |
| 3 | Dữ liệu hiệu chuẩn (nếu có) | **không** — đo trên chính thiết bị đó |

Trộn ba thứ này vào một ảnh flash duy nhất là sai lầm gốc: nó buộc mọi thiết bị dùng chung
bí mật, và làm mất hiệu chuẩn mỗi lần cập nhật.

## Tách vùng ngay từ partition table

Thiết kế `partitions.csv` sao cho ba loại dữ liệu nằm ở **namespace/partition khác nhau**:

| Vùng | Nội dung | Bị xoá khi |
|---|---|---|
| `nvs` (namespace `factory`) | serial, khoá, chứng chỉ, hiệu chuẩn | **không bao giờ** — kể cả factory reset |
| `nvs` (namespace `user`) | Wi-Fi credential, cấu hình người dùng | factory reset |
| `ota_0` / `ota_1` | application | OTA |

Đây là điều kiện để `factory reset` (`esp32-03-firmware-architecture/references/fault-tolerance.md`)
không xoá mất danh tính nhà máy. Quyết định này phải làm **trước khi ship lô đầu tiên** —
sau đó rất khó sửa.

## Quy trình nạp ở xưởng

1. **Nạp ảnh chung** (bootloader + partition table + app) — cùng một file cho cả lô.
2. **Nạp danh tính riêng**: sinh NVS partition riêng cho từng thiết bị rồi nạp vào offset của `nvs`.
   Công cụ chính thức: `nvs_partition_gen.py` (tạo từ file CSV), chạy được ở chế độ mã hoá.
3. **Hiệu chuẩn** (nếu sản phẩm cần): đo, ghi vào namespace `factory`.
4. **Test xuất xưởng** (mục dưới).
5. **Khoá thiết bị** (nếu có): eFuse, vô hiệu JTAG/UART download — **bước cuối cùng, không thể hoàn tác**.

Bước 5 thuộc `esp32-10-security/references/secure-boot-flash-encryption.md`.
**Trợ lý không bao giờ tự chạy lệnh eFuse** — chỉ trình bày để người vận hành tự chạy.

## Danh tính thiết bị

- Mỗi thiết bị một khoá/chứng chỉ riêng. Lộ một thiết bị không được làm lộ cả lô —
  đây là lớp 4 trong phân lớp phòng thủ của `esp32-10-security`.
- Khoá sinh ở đâu quyết định mức tin cậy: sinh **trên chính thiết bị** (private key không
  bao giờ rời chip) > sinh ở máy nạp có kiểm soát > sinh sẵn hàng loạt trong file.
- Phải có đường **thu hồi** một thiết bị cụ thể ở phía server.
- Bí mật của lô phải được ghi lại ở nơi có kiểm soát truy cập, không nằm trong thư mục dự án.

## Test xuất xưởng (EOL test)

Mục tiêu: bắt lỗi **phần cứng và lắp ráp**, không phải test lại logic (việc đó ở `esp32-08-testing`).

Yêu cầu với một EOL test tốt:
- **Tự động, không cần người đọc log.** Kết quả là PASS/FAIL nhìn thấy được (LED, buzzer,
  màn hình máy nạp) + một dòng ghi vào file kết quả kèm serial.
- **Chạy trong vài chục giây.** Test lâu thì công nhân sẽ bỏ qua.
- Phủ được: mọi ngoại vi có mặt (I2C scan thấy đủ thiết bị), radio (RSSI, hoặc gửi được một gói),
  nguồn (điện áp pin/nguồn trong dải), chân nối ra ngoài (loopback nếu có thể),
  bộ nhớ ngoài (flash/SD đọc ghi được).
- **Ghi kết quả vào NVS** (`factory`): đã test, ngày, phiên bản firmware test, kết quả từng mục.
  Sau này có thiết bị lỗi thì tra được nó đã pass những gì lúc xuất xưởng.

Firmware test có thể là một app riêng, hoặc một chế độ trong firmware chính kích hoạt bằng
điều kiện chỉ có ở xưởng (chân jumper, lệnh qua UART). Nếu là chế độ trong firmware chính:
đảm bảo nó **không thể** kích hoạt ngoài thực địa.

## Truy xuất nguồn gốc

Mỗi thiết bị xuất xưởng ghi lại được: serial, phiên bản firmware, ngày nạp, kết quả EOL test,
mã lô linh kiện nếu có. Khi có sự cố hàng loạt, đây là thứ cho biết **phạm vi ảnh hưởng** —
thu hồi 50 thiết bị của một lô thay vì cả 5000.
