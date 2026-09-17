# Wi-Fi

## Khởi tạo station — kèm xử lý lỗi init

```c
esp_err_t wifi_link_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) return err;            /* INIT_FAIL — partition NVS sai/thiếu */

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop");

    s_netif = esp_netif_create_default_wifi_sta();
    if (!s_netif) return ESP_ERR_NO_MEM;      /* INIT_FAIL — hết heap */

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_evt, NULL, &s_h_wifi), TAG, "reg wifi");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, ip_evt, NULL, &s_h_ip), TAG, "reg ip");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg), TAG, "cfg");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "storage");
    return esp_wifi_start();
}
```

`ESP_ERROR_CHECK` trên đường mạng chỉ dùng cho lỗi thật sự không thể tiếp tục (INIT_FAIL đã
xác định là bug). Lỗi có thể xảy ra lúc chạy phải trả về, không được abort cả thiết bị.

## Event handler — không blocking, không retry trực tiếp

```c
static void wifi_evt(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    switch (id) {
    case WIFI_EVENT_STA_START:
        net_post(NET_EVT_LINK_START);            /* task mạng sẽ gọi esp_wifi_connect() */
        break;
    case WIFI_EVENT_STA_DISCONNECTED: {
        const wifi_event_sta_disconnected_t *d = data;
        net_post_err(NET_EVT_LINK_DOWN, wifi_reason_to_class(d->reason), d->reason);
        break;                                   /* KHÔNG gọi esp_wifi_connect() ở đây */
    }
    default: break;
    }
}
```

Gọi `esp_wifi_connect()` ngay trong handler `DISCONNECTED` là lỗi phổ biến nhất: tạo bão
reconnect không backoff. Handler chỉ báo sự kiện; task mạng quyết định khi nào thử lại.

`IP_EVENT_STA_GOT_IP` mới là mốc "có mạng". Có liên kết Wi-Fi mà chưa có IP thì mọi socket fail.
Cũng phải xử lý `IP_EVENT_STA_LOST_IP` (DHCP hết hạn không gia hạn được) — coi như mất mạng.

## Mã lý do ngắt kết nối

| reason | Lớp lỗi | Ý nghĩa thực tế |
|---|---|---|
| 201 `NO_AP_FOUND` | LINK | sai SSID, AP ở 5 GHz, hoặc ngoài vùng phủ |
| 202 `AUTH_FAIL`, 15 `4WAY_HANDSHAKE_TIMEOUT`, 204 | AUTH (sau 3 lần liên tiếp) | gần như luôn là sai mật khẩu |
| 205 `CONNECTION_FAIL`, 203 `ASSOC_FAIL` | LINK | AP từ chối hoặc quá tải |
| 8 `ASSOC_LEAVE`, 2 `AUTH_EXPIRE` | LINK | AP chủ động ngắt |
| 200 `BEACON_TIMEOUT`, 39, 24 | LINK | sóng yếu, thiết bị đi xa |

ESP32 **không hỗ trợ băng 5 GHz**. Router dùng chung SSID cho 2.4 và 5 GHz vẫn kết nối được
nhưng hay rớt; khi gỡ lỗi nên tách SSID riêng cho 2.4 GHz.

## Quét và chọn AP

- Quét chủ động tốn thời gian và điện. Chỉ quét khi cần: nhiều lần LINK_FAIL liên tiếp, hoặc
  lúc provisioning.
- Ghim BSSID + kênh vào NVS sau lần kết nối thành công → lần sau connect nhanh hơn nhiều
  (quan trọng với thiết bị pin). Nhưng phải có đường quay lại quét đầy đủ nếu BSSID đó biến mất.
- Nhiều AP cùng SSID (mesh/repeater): chọn theo RSSI bằng `sta_cfg.sta.sort_method`.

## Provisioning — đưa SSID/mật khẩu vào thiết bị

Không bao giờ hard-code SSID/mật khẩu trong source đưa lên git.

Thứ tự ưu tiên: `wifi_provisioning` qua BLE (S3/C3/C6) hoặc SoftAP, lưu kết quả vào NVS.

Yêu cầu bắt buộc của luồng provisioning:
- Có timeout: bật provisioning mãi mãi là lỗ hổng bảo mật. Hết X phút không ai kết nối thì tắt.
- Có POP (proof of possession) hoặc mã ghép đôi; không để ai trong tầm sóng cũng ghi được cấu hình.
- Xác thực đầu vào: SSID ≤ 32 byte, mật khẩu ≤ 64 byte, cắt chuỗi đúng cách — đây là đầu vào
  từ bên ngoài (→ `esp32-10-security`).
- Kiểm chứng trước khi lưu: thử kết nối với thông tin mới; chỉ ghi NVS khi đã vào được AP.
  Không thì thiết bị tự khoá mình bằng cấu hình sai.
- Có nút giữ lâu để xoá cấu hình và quay lại provisioning, và có LED báo đang ở chế độ nào.

## SoftAP và chế độ APSTA

- `WIFI_MODE_APSTA` cho phép vừa phục vụ trang cấu hình vừa kết nối AP, nhưng AP và STA
  **phải cùng kênh** — kênh do AP bên ngoài quyết định. Nếu khách đang nối vào SoftAP mà STA
  đổi kênh, khách sẽ bị rớt. Đây là hành vi bình thường, cần nói trước trong UI.
- Đặt `max_connection` hợp lý (2–4); mỗi client chiếm heap.
- SoftAP mở không mật khẩu chỉ chấp nhận được trong lúc provisioning có giới hạn thời gian.

## Tiết kiệm điện

- `esp_wifi_set_ps(WIFI_PS_MIN_MODEM)` là mặc định hợp lý. `WIFI_PS_NONE` cho độ trễ thấp nhất
  nhưng tốn điện nhất. Chi tiết chu kỳ ngủ → `esp32-11-power-management`.

## Chẩn đoán

- `esp_wifi_sta_get_ap_info()` → RSSI. Dưới khoảng −75 dBm là yếu, sẽ rớt thất thường.
- Kết nối được nhưng socket fail: kiểm tra DNS (thử IP thuần), kiểm tra đã có IP chưa.
- Kết nối chập chờn đúng lúc bật tải công suất (relay, motor): sụt nguồn, không phải lỗi phần mềm.
- Rớt đều đặn đúng chu kỳ (ví dụ mỗi giờ): thường là DHCP lease hoặc AP xoay khoá — xử lý được
  bằng reconnect đúng cách, không phải bug.
