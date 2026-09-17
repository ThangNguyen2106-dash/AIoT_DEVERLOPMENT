# Ethernet

Chỉ dùng khi phần cứng thực sự có. Kiểm tra trước, đừng giả định:

| Cách nối | Chip hỗ trợ | Ghi chú |
|---|---|---|
| EMAC nội + PHY ngoài (LAN8720, IP101, RTL8201, DP83848) | **Chỉ ESP32 classic** | S2/S3/C3/C6 **không có EMAC**. Cần 9–11 chân RMII cố định, và nguồn clock 50 MHz đúng chiều. |
| Module SPI (W5500, ENC28J60, DM9051) | Mọi chip | Chậm hơn (vài Mbps thực tế) nhưng linh hoạt về chân; W5500 là lựa chọn mặc định tốt nhất. |
| USB-Ethernet | S2/S3 | hiếm dùng, không khuyến nghị cho sản phẩm |

Nếu người dùng nói "ESP32-S3 + Ethernet nội" thì đó là **INIT_FAIL do phần cứng** — phải nói
ra ngay, không viết code rồi để lỗi lúc chạy. Gán chân và nguồn clock → `esp32-02-hardware-analysis`.

## Khởi tạo W5500 qua SPI

```c
spi_device_interface_config_t devcfg = {
    .command_bits = 16, .address_bits = 8,
    .mode = 0, .clock_speed_hz = 20 * 1000 * 1000,   /* W5500 tới ~33 MHz, để dư */
    .spics_io_num = PIN_CS, .queue_size = 20,
};
eth_w5500_config_t w5500 = ETH_W5500_DEFAULT_CONFIG(SPI_HOST, &devcfg);
w5500.int_gpio_num = PIN_INT;                        /* dùng ngắt, đừng polling */

eth_mac_config_t  mac_cfg = ETH_MAC_DEFAULT_CONFIG();
eth_phy_config_t  phy_cfg = ETH_PHY_DEFAULT_CONFIG();
phy_cfg.reset_gpio_num = PIN_RST;
phy_cfg.phy_addr = 1;

esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500, &mac_cfg);
esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_cfg);
if (!mac || !phy) return ESP_ERR_NO_MEM;             /* INIT_FAIL */

esp_eth_config_t cfg = ETH_DEFAULT_CONFIG(mac, phy);
ESP_RETURN_ON_ERROR(esp_eth_driver_install(&cfg, &s_eth), TAG, "eth install");

/* W5500 KHÔNG có MAC address từ nhà sản xuất — phải nạp, thường lấy từ eFuse của ESP32 */
uint8_t macaddr[6];
ESP_RETURN_ON_ERROR(esp_read_mac(macaddr, ESP_MAC_ETH), TAG, "read mac");
ESP_RETURN_ON_ERROR(esp_eth_ioctl(s_eth, ETH_CMD_S_MAC_ADDR, macaddr), TAG, "set mac");

esp_netif_config_t ncfg = ESP_NETIF_DEFAULT_ETH();
s_netif = esp_netif_new(&ncfg);
ESP_RETURN_ON_ERROR(esp_netif_attach(s_netif, esp_eth_new_netif_glue(s_eth)), TAG, "attach");
ESP_RETURN_ON_ERROR(esp_eth_start(s_eth), TAG, "eth start");
```

Lỗi hay gặp lúc init (đều là **INIT_FAIL**, không retry được):
- `esp_eth_driver_install` fail hoặc PHY không phản hồi: sai chân CS/INT/RST, chưa cấp nguồn
  module, SPI clock quá cao, hoặc chưa nhả reset đủ lâu.
- Quên nạp MAC cho W5500 → có link nhưng DHCP không bao giờ trả IP.
- Với PHY RMII: sai chiều clock (ESP32 cấp clock ra GPIO0/16/17 hay PHY cấp vào) → link không
  bao giờ lên. Đây là lỗi phần cứng/cấu hình, không phải lỗi code.

## Sự kiện và vòng đời

```c
ETHERNET_EVENT_CONNECTED    /* có link vật lý — CHƯA có IP, chưa gửi được gì */
ETHERNET_EVENT_DISCONNECTED /* rút cáp / switch mất điện → LINK_FAIL */
IP_EVENT_ETH_GOT_IP         /* mới là READY-able */
IP_EVENT_ETH_LOST_IP
```

Ethernet rớt là chuyện thường: rút cáp, switch reboot, ai đó đá dây. Xử lý y hệt Wi-Fi rớt:
đổi state → BACKOFF → thử lại. Điểm khác là khi cáp bị rút, `CONNECTED` sẽ tự xuất hiện lại
ngay khi cắm lại — dùng nó làm *fast path* cắt backoff.

Nếu cáp rút mà không thấy event `DISCONNECTED`: module SPI đang chạy kiểu polling hoặc dây INT
chưa nối. Không có phát hiện link down thì thiết bị sẽ cố gửi vào hư không cho tới khi socket
timeout — chậm hơn nhiều.

## Ethernet + Wi-Fi cùng lúc (dự phòng)

ESP-IDF cho phép nhiều netif đồng thời; thứ tự ưu tiên định bởi route metric:

```c
esp_netif_set_default_netif(eth_netif);          /* ưu tiên Ethernet khi có */
/* khi Ethernet mất IP → chuyển default sang wifi_netif */
```

Quy tắc khi làm dự phòng:
- Chuyển link **không** tự động sửa các kết nối đang mở. Socket/TLS/MQTT đang bám netif cũ sẽ
  chết câm. Phải chủ động đóng và dựng lại session trên link mới.
- Có thời gian trễ trước khi chuyển (ví dụ Ethernet mất 10 s mới chuyển sang Wi-Fi), tránh
  nhảy qua lại liên tục khi cáp lỏng.
- Ghi log mỗi lần chuyển link kèm lý do; đây là thông tin chẩn đoán quan trọng ngoài hiện trường.

## Tĩnh hay DHCP

DHCP là mặc định. IP tĩnh khi hạ tầng yêu cầu:

```c
esp_netif_dhcpc_stop(s_netif);                   /* PHẢI stop trước khi set IP */
esp_netif_ip_info_t ip = { 0 };
ip.ip.addr = esp_ip4addr_aton("192.168.1.50");
ip.netmask.addr = esp_ip4addr_aton("255.255.255.0");
ip.gw.addr = esp_ip4addr_aton("192.168.1.1");
esp_netif_set_ip_info(s_netif, &ip);
/* IP tĩnh KHÔNG tự đặt DNS — phải set riêng, nếu không mọi hostname sẽ fail */
esp_netif_dns_info_t dns = { .ip.u_addr.ip4.addr = esp_ip4addr_aton("8.8.8.8"),
                             .ip.type = ESP_IPADDR_TYPE_V4 };
esp_netif_set_dns_info(s_netif, ESP_NETIF_DNS_MAIN, &dns);
```

Quên đặt DNS khi dùng IP tĩnh là nguyên nhân kinh điển của "ping được nhưng không lên cloud được".
