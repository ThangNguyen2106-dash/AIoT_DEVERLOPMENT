# Quy tắc code firmware ESP32

## Xử lý lỗi
- `ESP_ERROR_CHECK()` chỉ dùng cho lỗi lập trình không thể phục hồi lúc init (sai tham số,
  thiếu tài nguyên khi khởi động). Nó gọi `abort()` → reset thiết bị.
- Lỗi runtime có thể phục hồi (mất Wi-Fi, cảm biến không phản hồi, timeout) KHÔNG dùng
  `ESP_ERROR_CHECK` — phải trả lỗi lên và retry có backoff.
- Dùng mẫu goto-cleanup để không rò tài nguyên:

```c
static esp_err_t sensor_init(sensor_t *s)
{
    esp_err_t err = i2c_master_bus_add_device(bus, &cfg, &s->dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "add device failed: %s", esp_err_to_name(err));
        return err;
    }
    err = sensor_reset(s);
    if (err != ESP_OK) goto fail;
    return ESP_OK;
fail:
    i2c_master_bus_rm_device(s->dev);
    s->dev = NULL;
    return err;
}
```

## Logging
- Mỗi file một `static const char *TAG = "module";`
- Không `printf`. Dùng `ESP_LOGE/W/I/D/V`.
- Không log trong ISR (dùng `ESP_EARLY_LOGx` chỉ khi debug tạm, bỏ trước khi ship).
- Không log ở tần suất cao trong vòng lặp nóng — làm nghẽn UART và lệch timing.

## ISR
- Hàm ISR và mọi hàm nó gọi phải `IRAM_ATTR` nếu flash có thể bị cache-disable.
- Không: `malloc`, `free`, `printf`, mutex thường, hàm blocking, float (trên một số chip).
- Chỉ dùng biến thể `...FromISR()` và xử lý `pdTRUE` để `portYIELD_FROM_ISR()`.
- ISR chỉ đẩy dữ liệu vào queue / gửi task notification; xử lý nặng làm ở task.

## Bộ nhớ
- Ưu tiên cấp phát tĩnh hoặc cấp phát một lần lúc init. Tránh malloc/free lặp lại → phân mảnh heap.
- Buffer dùng cho DMA phải cấp bằng `heap_caps_malloc(n, MALLOC_CAP_DMA)` và căn chỉnh đúng.
- Biến chia sẻ giữa ISR và task: `volatile` + kiểu nguyên tử, hoặc bảo vệ bằng
  `portENTER_CRITICAL(_ISR)`.

## Cấu hình
- Gom toàn bộ pin/tham số board vào `main/board_config.h` (hoặc Kconfig), không rải `#define` khắp nơi.
- Giá trị tuỳ môi trường (SSID, endpoint, khoá) → NVS hoặc Kconfig, KHÔNG hard-code trong source.

## Thời gian
- Không `vTaskDelay(1)` để "chờ nhanh" — 1 tick thường là 10 ms.
- Delay ngắn dưới tick dùng `esp_rom_delay_us()`, nhưng đó là busy-wait, hạn chế dùng.
- Không dùng delay để đồng bộ giữa các task; dùng queue/semaphore/event group.
- **Tràn bộ đếm**: `esp_timer_get_time()` trả `int64_t` µs (an toàn); nhưng `xTaskGetTickCount()`
  và mọi biến `uint32_t` đếm ms **tràn sau ~49 ngày**. So sánh mốc thời gian luôn viết dạng
  `(int32_t)(now - deadline) >= 0`, không bao giờ `now >= deadline`. Firmware chạy nhiều tháng
  không ai nhìn — lỗi này chỉ lộ ra ngoài thực địa.
- **Dự phòng stack**: đo `uxTaskGetStackHighWaterMark` dưới tải thực, giữ dư **≥ 25%**.
  Đây là con số chuẩn của cả bộ skill (`esp32-08` DoD và `esp32-09` dùng lại con số này).
