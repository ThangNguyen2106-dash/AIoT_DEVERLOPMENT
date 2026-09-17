# BLE, ESP-NOW và chọn công nghệ

## Chọn công nghệ nào

| Nhu cầu | Nên dùng |
|---|---|
| Thiết bị nói chuyện với điện thoại, cấu hình tại chỗ | BLE GATT |
| Nhiều ESP32 nói chuyện trực tiếp, độ trễ thấp, không router | ESP-NOW |
| Lên internet / cloud qua hạ tầng sẵn có | Wi-Fi + MQTT hoặc HTTPS |
| Lên internet ở nơi không có Wi-Fi | Ethernet, hoặc modem cellular ngoài |
| Mạng lưới cảm biến pin, tầm xa trong nhà (chỉ C6/H2) | Thread hoặc Zigbee |
| Tầm rất xa, băng thông thấp | Không phải ESP32 — cần module LoRa ngoài |

Bluetooth Classic (SPP, A2DP) **chỉ có trên ESP32 classic**. S2 **không có Bluetooth**.
S3/C3/C6 chỉ có BLE. Kiểm tra trước khi thiết kế → `esp32-02-hardware-analysis`.

## BLE GATT server

### Khởi tạo và INIT_FAIL

- Bật BLE làm firmware phình rất nhiều (vài trăm KB flash). Kiểm tra partition trước; không đủ
  chỗ là INIT_FAIL phát hiện lúc build hoặc lúc OTA, không phải lúc chạy.
- `esp_bt_controller_init` / `esp_bluedroid_init` fail hầu hết là do hết heap hoặc chưa giải
  phóng vùng nhớ Classic BT (`esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT)`). Đây là
  INIT_FAIL: không retry, sửa cấu hình.
- Dùng NimBLE thay Bluedroid khi chỉ cần BLE: tiết kiệm đáng kể RAM và flash.

### Vòng đời kết nối

BLE cũng có state machine, đừng coi là "cắm là chạy":

```
IDLE → ADVERTISING → CONNECTED → (MTU exchange) → (bonding nếu cần) → READY
```

- Vào `READY` chỉ sau khi đã đàm phán MTU xong và (nếu yêu cầu) đã mã hoá/bonding xong. Gửi
  notify trước đó sẽ bị cắt cụt hoặc bị từ chối.
- `ESP_GATTS_DISCONNECT_EVT` → phải **bật lại advertising** (rất hay quên: thiết bị rớt một lần
  rồi không ai tìm thấy nữa cho tới khi reboot). Đọc `reason` trong event để phân loại: hết tầm
  (LINK) khác với xác thực thất bại (AUTH).
- Advertising vô hạn là lỗ hổng và tốn điện. Có timeout, có điều kiện bật (nhấn nút / chưa cấu hình).

### Truyền dữ liệu

- MTU mặc định 23 byte (payload 20 byte). Muốn gửi gói lớn phải đàm phán MTU (`esp_ble_gatt_set_local_mtu`)
  và **vẫn phải tự chia gói** ở tầng ứng dụng — phía kia có thể không chấp nhận MTU lớn.
- Notify hiệu quả hơn nhiều so với để client polling read. Indicate thì có xác nhận nhưng chậm hơn.
- Notify khi buffer controller đầy sẽ fail (`ESP_ERR_NO_MEM` / congest event). Phải xử lý
  `ESP_GATTS_CONGEST_EVT`: dừng gửi, đợi hết nghẽn, có hàng đợi giới hạn — y như buffering ở
  `connection-contract.md` §6. Gửi vòng lặp không kiểm tra là mất dữ liệu im lặng.

### Bảo mật

- Dữ liệu nhạy cảm phải yêu cầu bonding + encryption ở mức characteristic. Không để đặc tính
  ghi được cho mọi thiết bị lạ.
- Mọi ghi vào characteristic là **đầu vào không tin cậy từ bất kỳ ai trong tầm sóng**: kiểm tra
  độ dài, kiểu, dải giá trị trước khi dùng (→ `esp32-10-security`).
- Tắt BLE sau khi provisioning xong nếu không còn dùng: tiết kiệm điện và giảm bề mặt tấn công.

## ESP-NOW

Không cần router, độ trễ thấp, phù hợp điều khiển thời gian thực giữa các ESP32.

### Ràng buộc phải biết trước

- Payload tối đa 250 byte/gói. Cần hơn thì tự chia và ghép, kèm số thứ tự.
- **Tất cả thiết bị phải ở cùng kênh Wi-Fi.** Dùng chung với Wi-Fi station thì kênh do AP quyết
  định — đây là nguyên nhân số một khiến ESP-NOW "chạy lúc được lúc không". Xử lý: hoặc cố định
  kênh cho toàn hệ (không dùng station), hoặc để thiết bị phát quét/bám theo kênh của thiết bị nhận.
- Số peer có giới hạn (20 mã hoá / ~20 tổng tuỳ cấu hình). Thêm peer fail là INIT_FAIL do vượt hạn mức.
- Bật mã hoá (PMK/LMK) nếu dữ liệu quan trọng; mặc định là gửi thô, ai trong tầm cũng nghe được.

### Gửi không phải là đến

```c
static void espnow_send_cb(const uint8_t *mac, esp_now_send_status_t status)
{
    /* CHỈ nói là gói đã lên sóng và có/không có ACK ở tầng MAC. */
    if (status != ESP_NOW_SEND_SUCCESS) net_post_err(EVT_TX_FAIL, NET_FAIL_LINK, 0);
}
```

- Callback báo thành công/thất bại ở mức MAC, không phải mức ứng dụng. Đừng coi gửi là chắc đến.
- Retry có trần trong callback; retry vô hạn sẽ nghẽn cả hàng đợi gửi.
- `esp_now_send()` là bất đồng bộ: gọi liên tiếp quá nhanh sẽ trả `ESP_ERR_ESPNOW_NO_MEM`. Phải
  có hàng đợi và chỉ gửi gói tiếp theo sau khi có callback của gói trước (hoặc giới hạn số gói bay).
- Điều khiển cơ cấu chấp hành qua ESP-NOW: bắt buộc có số thứ tự + chống phát lại + hạn hiệu lực
  của lệnh. Không có thì một gói bị ghi lại và phát lại sẽ bật được thiết bị.
- Mất liên lạc với peer cũng là mất kết nối: có heartbeat và fail-safe theo thời gian, y như các
  link khác (`connection-contract.md` §5).
