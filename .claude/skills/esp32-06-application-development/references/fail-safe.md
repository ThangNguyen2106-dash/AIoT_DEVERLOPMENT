# Fail-safe: thiết bị hỏng thì về đâu

Driver bảo vệ **phần cứng** (`esp32-04-driver-development/references/actuators.md`).
File này nói về việc bảo vệ **quy trình**: khi một phần của hệ thống mất, thiết bị vẫn phải
làm điều an toàn, và phải nói cho người ta biết.

## Luật: trạng thái an toàn là quyết định nghiệp vụ, không phải mặc định kỹ thuật

"Tắt hết" không phải lúc nào cũng an toàn:

| Thiết bị | Mất cảm biến thì | Vì sao |
|---|---|---|
| Bơm tưới | **dừng** | tưới quá không cứu được, tràn thì hỏng |
| Quạt làm mát tủ điện | **chạy tiếp** ở mức mặc định | dừng quạt làm cháy thiết bị |
| Van khí | **đóng** | mặc định an toàn của ngành |
| Máy sưởi ấp trứng | **giữ công suất cuối** trong T phút rồi mới tắt | tắt ngay làm chết phôi |
| Đèn báo hiệu | **bật** | tắt là mất cảnh báo |

**Phải hỏi user** thiết bị này đúng ra phải về đâu. Đừng suy ra từ "tắt cho an toàn".
Đây là câu hỏi chặn khi có cơ cấu chấp hành gây hại được.

## Ba tầng lưới an toàn

Xây cả ba, không chọn một:

| Tầng | Ai thực hiện | Bắt được gì |
|---|---|---|
| 1. Phần cứng | pull-down/pull-up, relay thường mở, cầu chì | firmware chết hẳn, reset, mất nguồn |
| 2. Driver | kẹp giới hạn, watchdog thiết bị (`*_feed_watchdog`) | tầng ứng dụng treo hoặc gửi lệnh sai |
| 3. Ứng dụng (ở đây) | giám sát nghiệp vụ, timeout, degrade | cảm biến sai, mất mạng, logic sai điều kiện |

Tầng 1 là tầng duy nhất còn hoạt động khi firmware panic. Nếu trạng thái an toàn chỉ được
đảm bảo bằng code, thì **nó không được đảm bảo**.

## Bốn nguồn lỗi phải xử lý

### 1. Dữ liệu sai — nguy hiểm hơn thiếu dữ liệu
Cảm biến hỏng thường không im lặng; nó trả về số **trông có vẻ hợp lệ**. Kiểm:
- **Dải vật lý**: nhiệt độ nước −40..120 °C. Ngoài dải → bỏ mẫu, đếm.
- **Tốc độ đổi**: nước không tăng 40 °C trong 1 giây. Vượt → nghi ngờ, không dùng ngay.
- **Đứng im tuyệt đối**: giá trị không đổi một chút nào trong N phút thường là cảm biến chết
  hoặc dây đứt, không phải môi trường ổn định.
- **Đối chiếu chéo** nếu có hai nguồn (hai cảm biến, hoặc cảm biến với mô hình).

Mẫu bị loại phải **đếm và báo ra ngoài**. Bỏ im lặng thì thiết bị chạy bằng dữ liệu cũ mà
không ai biết.

### 2. Mất nguồn dữ liệu
Driver trả ERROR N lần liên tiếp → coi như mất. Ngưỡng N và thời gian chờ là quyết định
nghiệp vụ, khai báo trong cấu hình chứ không hard-code.
Có giá trị cuối còn dùng được không, và dùng được **trong bao lâu**? Sau đó thì sao?

### 3. Mất liên lạc với server
Offline là **trạng thái hợp lệ**, không phải lỗi (`esp32-05-connectivity`).
Phải trả lời: điều khiển cục bộ vẫn chạy chứ? Lệnh cuối từ server còn hiệu lực bao lâu?
Thiết bị có được phép tự quyết không?

Lệnh từ server phải có **hạn dùng**. Không có hạn thì một lệnh "bật bơm" gửi trước khi mất mạng
sẽ chạy mãi mãi.

### 4. Mất điện giữa chừng
Sau reset, thiết bị phải vào trạng thái an toàn **trước** khi khôi phục trạng thái cũ.
Khôi phục "đang bơm" từ NVS rồi bật bơm ngay khi boot là cách làm ngập nhà.

Quy tắc: **không tự động khôi phục hành động nguy hiểm sau reset.** Khôi phục *cấu hình*,
không khôi phục *hành động đang chạy*. Muốn chạy lại thì phải qua điều kiện an toàn một lần nữa.

## Degrade có trật tự

Mất một phần không được làm mất tất cả. Xếp chức năng theo thứ tự hy sinh:

```
Bỏ trước:  telemetry chi tiết → log từ xa → tính năng tiện ích
Giữ tới cùng: an toàn cơ cấu chấp hành → điều khiển cục bộ → cảnh báo tại chỗ
```

Chức năng an toàn **không bao giờ** phụ thuộc vào mạng, vào server, hay vào việc parse thành công
một gói tin từ ngoài.

## Báo ra ngoài

Thiết bị vào trạng thái fail-safe mà không ai biết thì chỉ là hỏng chậm hơn. Mỗi lần degrade:
- **Tại chỗ**: LED/buzzer/màn hình — người đứng cạnh phải thấy.
- **Từ xa**: telemetry + counter, gửi ngay khi có mạng trở lại.
- **Lưu lại**: ghi vào NVS để còn sau reset (`esp32-12-release/references/remote-diagnostics.md`).

## Kiểm chứng

Fail-safe chưa thử = fail-safe không tồn tại. Tối thiểu, với mỗi hàng trong bảng fail-safe
ở SKILL.md: **rút dây cảm biến**, **tắt Wi-Fi**, **cắt nguồn giữa chừng** — và xác nhận thiết bị
về đúng chỗ đã thiết kế, trong đúng thời gian đã hứa.
Hiện thực thành test tự động → `esp32-08-testing/references/fault-injection.md`.
