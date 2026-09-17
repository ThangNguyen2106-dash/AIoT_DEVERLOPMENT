# Hệ thống file: LittleFS, SPIFFS, SD

## Trước hết: có thật sự cần filesystem không?

NVS đủ cho: cấu hình, hiệu chuẩn, bộ đếm, cờ trạng thái — dữ liệu nhỏ, có khoá, ghi thưa.
Dùng NVS được thì **đừng** dựng filesystem: nó tốn flash, tốn RAM, và thêm một cách hỏng.

Cần filesystem khi: log nhiều bản ghi, buffer offline lớn, tài nguyên (font, ảnh, trang web),
file do người dùng đưa vào.

## Chọn cái nào

| | LittleFS | SPIFFS | FAT trên SD |
|---|---|---|---|
| Chống mất điện | **có** (copy-on-write) | **không** — hỏng FS là chuyện thường | kém, phụ thuộc thẻ |
| Wear leveling | có | có | tuỳ thẻ |
| Thư mục thật | có | không (tên phẳng) | có |
| Tốc độ khi đầy | ổn định | **tụt rất mạnh** khi > 80% | ổn |
| Dung lượng | flash nội (MB) | flash nội | GB |
| Trong IDF | qua component registry | có sẵn | có sẵn |

**Mặc định chọn LittleFS.** SPIFFS chỉ dùng khi đang có sẵn và chỉ đọc.
Dữ liệu quan trọng mà đặt trên SPIFFS với thiết bị hay mất điện là mất dữ liệu có hẹn trước.

SD card: bắt buộc khi cần GB hoặc cần rút ra đọc trên máy tính. Nhưng thẻ SD là linh kiện
hay hỏng nhất trong hệ thống — coi nó là **có thể mất bất cứ lúc nào**, không phải nơi lưu
dữ liệu duy nhất.

## Quy tắc chống hỏng dữ liệu

1. **Ghi nguyên tử bằng file tạm**, không ghi đè trực tiếp:
   ```
   ghi vào data.tmp → fflush + fsync → đóng → rename("data.tmp", "data.json")
   ```
   Mất điện giữa chừng → `data.json` cũ vẫn nguyên vẹn. Ghi đè trực tiếp thì mất cả hai.
2. **Đóng file ngay sau khi ghi.** File mở khi mất điện là file có nguy cơ hỏng.
3. **Giới hạn kích thước và số file.** Log không giới hạn sẽ làm đầy FS, và FS đầy làm
   thiết bị hỏng theo cách rất khó đoán. Xoay vòng: `log.1`, `log.2`, giữ N file.
4. **Kiểm tra dung lượng trước khi ghi**, không ghi rồi bắt lỗi:
   ```c
   size_t total = 0, used = 0;
   esp_littlefs_info(label, &total, &used);
   if (total - used < need + margin) { rotate_or_drop(); }
   ```
5. **Mount thất bại phải xử lý được.** Thiết bị vẫn phải boot và chạy chức năng chính khi FS hỏng
   — format lại và tiếp tục, hoặc chạy ở chế độ không lưu. Treo ở `ESP_ERROR_CHECK(mount)`
   biến một FS hỏng thành một thiết bị chết.

## Cấu hình mount

```c
esp_vfs_littlefs_conf_t conf = {
    .base_path       = "/data",
    .partition_label = "storage",
    .format_if_mount_failed = true,   /* cân nhắc: tự phục hồi, nhưng mất dữ liệu im lặng */
    .dont_mount      = false,
};
```

`format_if_mount_failed = true` đánh đổi: thiết bị tự phục hồi thay vì chết, nhưng dữ liệu
biến mất mà không ai biết. Nếu bật, **phải log ERROR và đếm** để thấy được qua telemetry
(`esp32-12-release/references/remote-diagnostics.md`). Dữ liệu không thể mất (hiệu chuẩn,
danh tính) thì **đừng để trên filesystem** — để ở NVS namespace `factory`.

Partition cho filesystem khai báo trong `partitions.csv`
(`esp32-01-project-init/references/partitions.md`). Kích thước phải chốt **trước khi ship**:
đổi kích thước partition trên thiết bị đã triển khai là thao tác bị cấm.

## Buffer offline trên filesystem

Dùng khi mất mạng lâu và không được mất dữ liệu (`esp32-05-connectivity` sở hữu chính sách buffer):

- Một file một lô bản ghi, đặt tên theo timestamp — dễ xoá lô cũ nhất khi đầy.
- **Chỉ xoá sau khi server xác nhận nhận được**, không xoá lúc gửi.
- Ghi theo lô (N bản ghi hoặc T giây), không ghi từng bản ghi một: mỗi lần ghi là một chu kỳ
  xoá/ghi flash, và flash có giới hạn số lần (`storage-nvs.md`).
- Có trần cứng: đầy thì bỏ **bản ghi cũ nhất** và **đếm số bản ghi đã bỏ** — mất dữ liệu
  có ghi nhận thì chấp nhận được; mất im lặng thì không.

## Hiệu năng

Flash nội chậm hơn RAM hàng nghìn lần, và thao tác ghi **tắt cache** — làm gián đoạn code
đang chạy từ flash (`esp32-09/references/timing.md`). Không ghi file trong đường thời gian thực;
đẩy qua queue cho một task riêng ghi.
