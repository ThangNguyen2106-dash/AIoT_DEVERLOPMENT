# API vỡ theo phiên bản ESP-IDF

Đọc trước khi sinh bất kỳ code ngoại vi nào. **Không biết phiên bản → hỏi, đừng sinh code.**

```bash
idf.py --version            # hoặc
cat $IDF_PATH/version.txt
grep platform platformio.ini   # platform = espressif32@6.x  → IDF 5.1.x
```

Arduino-ESP32 core 3.x dựng trên IDF v5.1; core 2.x dựng trên IDF v4.4.
Khi user dùng PlatformIO, phiên bản `platform` mới là thứ quyết định, không phải con số IDF
cài trên máy.

## Nguyên tắc

1. **Mặc định giả định IDF v5.x** khi user không nói gì, nhưng **phải nêu giả định đó ra**.
2. Mỗi khối code sinh ra cho ngoại vi có bảng dưới đây đều ghi một dòng đầu:
   `/* ESP-IDF >= 5.x */`
3. API cũ vẫn biên dịch được ở v5.x (kèm cảnh báo deprecated) **cho tới khi bị gỡ hẳn** —
   đừng sửa code cũ của user sang API mới nếu user không yêu cầu. Nêu là được.
4. Bảng này là **chỉ dẫn tra cứu**, không phải nguồn chân lý. Nghi ngờ → mở header thật trong
   `$IDF_PATH/components/` và xác nhận, hoặc yêu cầu user chạy `idf.py build` để kiểm.

## Bảng API vỡ

| Ngoại vi | IDF v4.x | IDF v5.x | Vỡ từ |
|---|---|---|---|
| **I2C** | `driver/i2c.h`, `i2c_master_write_to_device()` | `driver/i2c_master.h`, bus/device handle | v5.2 (cũ deprecated) |
| **ADC oneshot** | `driver/adc.h`, `adc1_get_raw()`, `adc1_config_width()` | `esp_adc/adc_oneshot.h`, handle-based | v5.0 |
| **ADC continuous** | `driver/adc.h` + I2S DMA | `esp_adc/adc_continuous.h` | v5.0 |
| **ADC calibration** | `esp_adc_cal.h`, `esp_adc_cal_characterize()` | `esp_adc/adc_cali.h` + `adc_cali_scheme.h` | v5.0 |
| **RMT** | `driver/rmt.h` | `driver/rmt_tx.h` / `rmt_rx.h`, encoder-based | v5.0 |
| **GPTimer** | `driver/timer.h`, `timer_group_t` | `driver/gptimer.h` | v5.0 |
| **MCPWM** | `driver/mcpwm.h` | `driver/mcpwm_prelude.h`, timer/operator/comparator | v5.0 |
| **Pulse counter** | `driver/pcnt.h` | `driver/pulse_cnt.h` | v5.0 |
| **I2S** | `driver/i2s.h` | `driver/i2s_std.h` / `i2s_pdm.h` / `i2s_tdm.h` | v5.0 |
| **DAC** | `driver/dac.h` | `driver/dac_oneshot.h` / `dac_continuous.h` | v5.1 |
| **Temp sensor** | `driver/temp_sensor.h` | `driver/temperature_sensor.h` | v5.0 |
| **Network iface** | `tcpip_adapter.h` | `esp_netif.h` | gỡ hẳn ở v5.0 |
| **MQTT config** | phẳng: `.uri`, `.username` | lồng: `.broker.address.uri`, `.credentials.username` | v5.0 |
| **HTTPS OTA** | `esp_https_ota(&http_config)` | `esp_https_ota(&ota_config)` với `.http_config` bên trong | v5.0 |
| **Hall sensor** | `hall_sensor_read()` | **đã gỡ** | v5.0 |

Ổn định qua v4.4 → v5.x, dùng được không cần phân nhánh: `driver/gpio.h`, `driver/uart.h`,
`driver/spi_master.h`, `driver/ledc.h`, `driver/twai.h`, `esp_timer.h`, `esp_event.h`,
`nvs_flash.h`, `esp_log.h`, `esp_wifi.h` (API chính), FreeRTOS API.

## Bẫy hay gặp khi sinh code

- **Trộn hai thế hệ API cùng một ngoại vi** trong một dự án: `driver/i2c.h` và
  `driver/i2c_master.h` cùng chạm một port sẽ xung đột. Chọn một, giữ nhất quán toàn repo.
- **Chép ví dụ từ blog cũ**: phần lớn ví dụ I2C/ADC/RMT trên mạng là API v4.x. Biên dịch được
  ở v5.x nhưng sinh cảnh báo, và sẽ hỏng ở phiên bản gỡ hẳn.
- **`esp_crt_bundle_attach`** cần `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y`; thiếu nó thì link lỗi,
  không phải lỗi runtime.
- **Ngoại vi không có trên chip** quan trọng hơn cả phiên bản: C3/C6 không có DAC, C3 không có
  I2S TDM đầy đủ, S2 không có Bluetooth. Kiểm năng lực chip ở
  `esp32-02-hardware-analysis/references/chip-matrix.md` **trước** khi kiểm phiên bản API.

## Khi phải hỗ trợ nhiều phiên bản

```c
#include "esp_idf_version.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    /* driver/i2c_master.h */
#else
    /* driver/i2c.h */
#endif
```

Chỉ làm khi dự án **thật sự** phải build trên nhiều phiên bản (thư viện dùng chung, CI đa
version). Với firmware sản phẩm: ghim một phiên bản IDF ở `esp32-01-project-init` và bỏ hẳn
nhánh cũ — mỗi `#if` là một đường code không ai test.
