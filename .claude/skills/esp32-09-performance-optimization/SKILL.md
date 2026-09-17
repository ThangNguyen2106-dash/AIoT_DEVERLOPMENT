---
name: esp32-09-performance-optimization
description: Tối ưu hiệu năng và tài nguyên firmware ESP32 khi code đã đúng — đo và giảm RAM/heap/stack, chống phân mảnh, thu nhỏ binary cho vừa slot OTA, tăng tốc vòng lặp nóng, IRAM và cache, DMA, PSRAM, tần số CPU, độ trễ và jitter, throughput bus. Dùng khi hết RAM, binary quá lớn, xử lý không kịp thời hạn, hoặc thông lượng thấp hơn mong đợi.
---

# 09 — Performance Optimization

## Điều kiện vào
Chưa rõ chip / framework / phiên bản IDF → chạy Context Gate ở `esp32-firmware` trước.
Cổng an toàn và ngân sách context của `esp32-firmware` áp dụng nguyên vẹn tại đây.

Làm cho firmware **đã đúng** chạy vừa tài nguyên và kịp thời hạn.
Đầu ra là **số trước / số sau** kèm đánh đổi, không phải một loạt thay đổi "cho nhanh hơn".

## Nguyên tắc tối thượng: không có số thì không sửa

Tối ưu không đo là đoán, và đoán trên firmware thì thường sai: nút thắt gần như luôn nằm ở
chỗ khác với nơi người ta nghĩ. Ba luật:

1. **Đo trước, sửa sau, đo lại.** Mỗi thay đổi phải có số trước và số sau, cùng điều kiện tải.
   Không có số trước → việc đầu tiên là đo, không phải sửa.
2. **Một thay đổi một lần.** Sửa 5 thứ cùng lúc thì nhanh hơn cũng không biết nhờ cái nào,
   và chậm đi cũng không biết tại cái nào.
3. **Công bố đánh đổi.** Mọi tối ưu đều đổi một thứ lấy thứ khác — tốc độ đổi RAM, RAM đổi
   flash, throughput đổi điện năng, độ trễ thấp đổi jitter cao. Không nêu đánh đổi là giấu chi phí.

**Điều kiện tiên quyết**: firmware phải **đúng** trước đã. Rò heap, crash, dữ liệu sai là
việc của `esp32-07-debugging`, không phải tối ưu. Tối ưu code còn sai chỉ làm nó sai nhanh hơn.

## Phân loại vấn đề — vào đúng nhánh

| Triệu chứng | Loại | Reference |
|---|---|---|
| `malloc` thất bại, `free heap` thấp, cấp phát khối lớn hỏng | bộ nhớ | `references/memory.md` |
| Stack overflow lúc tải cao, không biết đặt stack bao nhiêu | stack | `references/memory.md` |
| App không vừa slot OTA, `idf.py size` vượt ngưỡng | kích thước | `references/binary-size.md` |
| Task trễ deadline, jitter lớn, ISR bị trễ | thời gian thực | `references/timing.md` |
| Vòng lặp xử lý chậm, CPU 100%, watchdog suýt bắt | CPU | `references/timing.md` |
| Bus/mạng chậm hơn lý thuyết nhiều, CPU bận vì copy | throughput | `references/throughput.md` |
| Chưa biết nút thắt ở đâu | đo | `references/measure.md` |

Chỉ đọc reference của nhánh đang xét. Không nạp cả thư mục.

## Quy trình 6 bước

| # | Bước | Đầu ra | Bẫy |
|---|---|---|---|
| 1 | **Ràng buộc** | con số phải đạt và **vì sao** (slot OTA 1.5 MB; deadline 10 ms; 50 mẫu/s) | "cho nó nhanh hơn" không phải ràng buộc |
| 2 | **Đo hiện trạng** | số liệu dưới **tải thực**, không phải lúc idle | đo lúc Wi-Fi chưa kết nối rồi kết luận về RAM |
| 3 | **Khoanh nút thắt** | một nguyên nhân chiếm phần lớn chi phí, có số chứng minh | tối ưu cái chiếm 2% tổng thời gian |
| 4 | **Sửa một thứ** | thay đổi nhỏ nhất chạm đúng nút thắt | refactor kèm theo |
| 5 | **Đo lại** | cùng điều kiện tải, so với bước 2 | đổi điều kiện đo rồi khoe cải thiện |
| 6 | **Công bố đánh đổi** | bảng: được gì / mất gì / rủi ro mới | im lặng đánh đổi độ tin cậy lấy tốc độ |

## Thứ tự ưu tiên khi tối ưu

Rẻ và an toàn trước, đắt và rủi ro sau. Đừng bắt đầu từ cuối bảng:

| # | Biện pháp | Chi phí rủi ro |
|---|---|---|
| 1 | Bỏ thứ không dùng (component, log, buffer thừa, task rỗi) | gần như không |
| 2 | Đổi cấu hình (mức log, kích thước buffer, `-Os`, tần số CPU) | thấp, hoàn tác dễ |
| 3 | Đổi cách cấp phát (tĩnh / một lần lúc init thay cho malloc lặp) | thấp |
| 4 | Đổi thuật toán hoặc cấu trúc dữ liệu ở chỗ nóng | trung bình |
| 5 | Dùng DMA thay copy bằng CPU | trung bình, phải kiểm ràng buộc bộ nhớ |
| 6 | `IRAM_ATTR`, đổi bố trí bộ nhớ, PSRAM | cao — đổi RAM lấy tốc độ, dễ vỡ chỗ khác |
| 7 | Viết lại bằng assembly / chạm thanh ghi thẳng | rất cao, gần như không bao giờ đáng |

## Những thứ KHÔNG được làm nhân danh hiệu năng

| Bị cấm | Vì sao |
|---|---|
| Tắt hoặc nới watchdog vì task chạy lâu | che vấn đề thời gian thực; xem `esp32-03/references/fault-tolerance.md` |
| Tắt brownout detector | lỗi nguồn, không phải hiệu năng |
| Bỏ kiểm tra lỗi / bỏ validate dữ liệu để tiết kiệm chu kỳ | đổi độ đúng lấy tốc độ |
| Bỏ TLS hoặc hạ mức bảo mật để tăng throughput | quyết định của `esp32-10-security`, không phải của skill này |
| Tăng priority task để "kịp deadline" | thường gây starvation chỗ khác; sửa ở `esp32-03` |
| Giảm stack để tiết kiệm RAM mà không đo high-water mark | đổi crash lấy vài KB |
| Ép buffer DMA vào PSRAM khi ngoại vi không DMA được từ PSRAM | hỏng im lặng hoặc dữ liệu rác |

## Mẫu báo cáo

```
## Ràng buộc
<số phải đạt> — vì <lý do: slot OTA / deadline / tần suất lấy mẫu>

## Đo hiện trạng (tải thực: <mô tả tải>)
| Chỉ số | Giá trị | Công cụ |
|---|---|---|
| free heap (min) | 38 KB | esp_get_minimum_free_heap_size |
| largest free block | 12 KB | heap_caps_get_largest_free_block |
| mqtt_task high-water | 480 B | uxTaskGetStackHighWaterMark |
| app size | 1.61 MB / 1.5 MB slot | idf.py size |

## Nút thắt
<một nguyên nhân + số chứng minh nó chiếm phần lớn chi phí>

## Thay đổi và kết quả
| Thay đổi | Trước | Sau | Đánh đổi |
|---|---|---|---|
| Log mặc định VERBOSE → INFO | 1.61 MB | 1.48 MB | mất chi tiết khi gỡ lỗi tại chỗ |
| Buffer JSON 4 KB: stack → cấp một lần lúc init | high-water 480 B | 2.9 KB | +4 KB heap thường trú |

## Đã cân nhắc nhưng từ chối
<tối ưu nào bị loại và vì sao — thường vì đánh đổi không chấp nhận được>

## Cách kiểm chứng lại
<đo bằng gì, dưới tải nào, số kỳ vọng>
```

## References
- `references/measure.md` — đo cái gì bằng công cụ nào; cách đo đúng dưới tải thực
- `references/memory.md` — heap, stack, phân mảnh, bố trí bộ nhớ, PSRAM
- `references/binary-size.md` — `idf.py size`, cắt component, mức log, `-Os`, slot OTA
- `references/timing.md` — CPU, ISR, IRAM/cache, tần số CPU, jitter, deadline
- `references/throughput.md` — DMA, kích thước giao dịch, bus, mạng, zero-copy

## Không thuộc scope
- Rò heap, crash, dữ liệu sai — firmware **chưa đúng** → `esp32-07-debugging`
- Giảm tiêu thụ điện (khác với giảm thời gian chạy) → `esp32-11-power-management`
- Đổi số task, priority, cơ chế đồng bộ → `esp32-03-firmware-architecture`
- Đổi cách driver nói chuyện với thiết bị → `esp32-04-driver-development`
- Hạ mức bảo mật để nhanh hơn → `esp32-10-security` quyết định, không phải skill này
- Binary không vừa vì partition chia sai → `esp32-01-project-init`

## Đầu ra
Báo cáo theo mẫu: ràng buộc, số trước, nút thắt, thay đổi, số sau, đánh đổi, cách kiểm chứng.
Chưa đo được → đầu ra là **kế hoạch đo** (đo gì, bằng gì, dưới tải nào), không phải bản sửa.
