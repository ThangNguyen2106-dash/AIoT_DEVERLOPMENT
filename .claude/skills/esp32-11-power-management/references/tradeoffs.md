# Bảng đánh đổi sẵn và câu hỏi bắt buộc

Dùng ở bước 8 của quy trình. Mọi đề xuất tiết kiệm điện phải xuất hiện trong báo cáo kèm bốn cột:
**Power / Latency / Reliability / Functionality**. Không có bảng này thì chưa xong việc.

## Câu hỏi phải hỏi trước khi đề xuất bất cứ gì

Không trả lời được thì **hỏi người dùng**, không tự chọn hộ. Câu trả lời quyết định kỹ thuật nào
hợp lệ, chứ không phải kỹ thuật nào tiết kiệm nhất.

1. Thiết bị có cần **nhận lệnh từ xa** không? Chậm nhất bao lâu là chấp nhận được?
2. Có **sự kiện tại chỗ** cần phản ứng nhanh không (báo động, nút bấm, ngưỡng nguy hiểm)?
   Chậm nhất bao lâu?
3. Dữ liệu đo **được phép mất** không? Mất bao nhiêu là chấp nhận được (1 mẫu? 1 giờ? không mẫu nào)?
4. Có yêu cầu pháp lý/hợp đồng nào về tần suất đo hoặc lưu trữ không?
5. Thiết bị có **cơ cấu chấp hành** không? Trạng thái an toàn của nó là gì?
6. Có cần **OTA ngoài hiện trường** không? Cửa sổ cập nhật ra sao?
7. Môi trường sóng: cạnh router hay ở biên vùng phủ? AP do ai quản lý (có đổi kênh/roaming không)?
8. Tuổi thọ pin mục tiêu và dung lượng pin thực tế?

Câu 1–3 quyết định gần như toàn bộ thiết kế. Bỏ qua chúng rồi tối ưu là làm hỏng sản phẩm một
cách có kỹ thuật.

## Bảng đánh đổi theo kỹ thuật

Cột Power là **hướng và độ lớn tương đối**; số thật phải đo trên thiết bị (`power-budget.md`).

| Kỹ thuật | Power | Latency | Reliability | Functionality |
|---|---|---|---|---|
| Deep sleep thay vì active | giảm rất lớn | tăng bằng cả chu kỳ ngủ | mất kết nối mỗi chu kỳ; RTC mem có thể mất | **không nhận được lệnh khi ngủ**; không log liên tục |
| Light sleep thay vì active | giảm lớn | gần như không đổi | timing ngoại vi lệch; UART có thể mất byte | driver nhạy timing phải giữ power lock |
| Auto light sleep (`CONFIG_PM_ENABLE`) | giảm trung bình | ngắt trễ hơn | bit-bang/I2S/RMT có thể sai nếu thiếu lock | — |
| Hạ tần số CPU | giảm nhỏ, đôi khi **không giảm** | xử lý lâu hơn | vỡ timing driver | — |
| Hibernation | giảm thêm vài µA | như deep sleep | **mất RTC memory** → mất BSSID, buffer | mỗi lần thức là khởi động lạnh hoàn toàn |
| Giãn chu kỳ đo | giảm tỉ lệ thuận | dữ liệu thưa hơn | bỏ sót sự kiện giữa hai lần đo | có thể vi phạm yêu cầu nghiệp vụ/quy định |
| Gom mẫu, gửi thưa | giảm lớn | dữ liệu lên cloud trễ tới cả chu kỳ gộp | mất cả lô nếu mất nguồn (buffer RAM) | cảnh báo tức thời cần đường riêng |
| Ghim BSSID/kênh | giảm lớn | kết nối nhanh hơn | AP đổi kênh ⇒ fail nếu **thiếu fallback** | — |
| IP tĩnh, bỏ DHCP | giảm vừa | nhanh hơn | xung đột IP; không chạy trên mạng lạ | khó triển khai ở mạng khách hàng |
| `WIFI_PS_MAX_MODEM` | giảm vừa khi online | lệnh trễ tới vài giây | mất gói broadcast/multicast | điều khiển tức thời không còn đúng |
| Giảm TX power | giảm khi TX | — | **tăng retry ở biên sóng → có thể tốn hơn** | giảm vùng phủ |
| Cắt nguồn cảm biến | giảm tuỳ cảm biến | thêm `t_startup` mỗi lần đo | thêm một điểm lỗi init mỗi chu kỳ | mất bộ lọc nội; cảm biến cần làm nóng sẽ **sai số liệu** |
| ULP/LP core canh ngưỡng | giảm rất lớn | phản ứng nhanh hơn deep sleep thuần | code ULP khó test, lỗi khó thấy | logic rất hạn chế; chi phí phát triển cao |
| Giảm mức log | giảm nhỏ | — | — | **mất khả năng chẩn đoán từ xa** |
| Tắt Wi-Fi giữa các chu kỳ | giảm lớn | không nhận lệnh giữa chừng | — | như deep sleep về mặt downlink |
| Buffer dữ liệu vào RTC mem | giảm lớn (ít lần radio) | dữ liệu trễ | mất khi mất nguồn | — |
| Buffer xuống flash | giảm vừa | dữ liệu trễ | bền hơn nhiều | mòn flash, tốn thêm năng lượng ghi |

## Ba đánh đổi luôn bị đánh giá thấp

**1. Downlink chết.** Deep sleep hoặc tắt Wi-Fi giữa chu kỳ khiến server không gọi tới thiết bị
được. Người dùng thường vẫn nghĩ họ "điều khiển được thiết bị". Phải nói thẳng con số: lệnh tới
chậm nhất là bao nhiêu phút.

**2. Mất khả năng OTA.** Thiết bị ngủ 99% thời gian, cửa sổ online 3 giây mỗi 30 phút thì OTA
gần như không thực hiện được. Phải thiết kế cửa sổ bảo dưỡng ngay từ đầu:
- cờ "có bản cập nhật" lấy về trong cửa sổ bình thường, rồi thiết bị chủ động ở lại online lâu hơn;
- hoặc nút bấm/đường vào chế độ bảo dưỡng tại chỗ;
- hoặc lịch cố định (ví dụ 02:00 mỗi ngày online 5 phút).
Thiếu cái này, thiết bị ngoài hiện trường vĩnh viễn không vá được. → `esp32-12-release`.

**3. Mất khả năng chẩn đoán.** Giảm log, bỏ telemetry trạng thái, bỏ đếm lỗi để tiết kiệm điện
làm mọi sự cố hiện trường trở thành không truy được. Giữ tối thiểu: lý do reset lần trước,
`connect_failures`, `radio_ms_total`, vbat, số chu kỳ. Chi phí điện của mấy con số này gần như
bằng không so với giá trị chẩn đoán.

## Những đánh đổi cần người dùng xác nhận rõ ràng

Không được tự quyết, kể cả khi ngân sách pin đang thiếu:

- Giảm tần suất đo dưới mức yêu cầu nghiệp vụ hoặc quy định.
- Bỏ/giảm TLS, bỏ xác thực, bỏ đồng bộ thời gian để rút ngắn pha radio.
- Bỏ retry hoặc bỏ buffer khi gửi thất bại (⇒ mất dữ liệu im lặng).
- Kéo dài listen interval / chu kỳ kết nối làm thiết bị không nhận lệnh trong hàng phút.
- Deep sleep khi cơ cấu chấp hành đang ở trạng thái không an toàn.
- Bỏ cửa sổ OTA / chế độ bảo dưỡng.

Và những thứ **không bao giờ** đề xuất như biện pháp tiết kiệm điện:
- Tắt brownout detector (đây là lưới an toàn phần cứng — xem `esp32-07-debugging`).
- Tắt watchdog.
- Bỏ kiểm tra lỗi để rút ngắn thời gian chạy.

## Khi ngân sách không đạt

Nếu tính ra không đủ pin, thứ tự xử lý đúng — dừng lại ở bước đầu tiên đủ giải quyết:

1. **Kiểm tra lại phép đo và phần cứng** (dòng rò thường là thủ phạm) → `peripheral-power.md`.
2. **Rút ngắn pha radio** — gần như luôn là khoản lớn nhất → `wifi-power.md`.
3. **Giãn chu kỳ gửi** (gom mẫu) trong khi giữ nguyên chu kỳ **đo**.
4. **Đổi pin hoặc thêm năng lượng** (pin lớn hơn, hoá học khác, pin mặt trời).
5. **Đổi kiến trúc truyền thông** (BLE/ESP-NOW + gateway thay cho Wi-Fi trực tiếp).
6. **Giãn chu kỳ đo** — chỉ khi nghiệp vụ cho phép, và phải hỏi.
7. **Hạ yêu cầu tuổi thọ** — đây là quyết định của người dùng, không phải của mình.

Không bao giờ nhảy thẳng xuống bước 6–7 để đạt con số. Nếu sau tất cả vẫn không đạt, kết luận
trung thực là: *"với yêu cầu hiện tại, thiết kế này đạt được X tháng thay vì Y. Để đạt Y cần
hy sinh một trong các mục sau: ..."* — kèm danh sách cụ thể. Đó là câu trả lời đúng, không phải
báo cáo một con số đẹp bằng cách âm thầm cắt chức năng.
