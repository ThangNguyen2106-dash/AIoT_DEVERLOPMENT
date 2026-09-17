# Hợp đồng driver: interface, timeout, lỗi, recovery, state, log, test

Áp dụng cho mọi driver, kể cả driver một chân GPIO. Phần nào thấy thừa với driver đang viết
thì bỏ, nhưng phải bỏ có ý thức, không phải vì quên.

## 1. Interface

Header công khai phải trả lời được 6 câu, viết thẳng dạng comment:

```c
/**
 * Đọc nhiệt độ và độ ẩm.
 *
 * Context : task (KHÔNG gọi từ ISR)
 * Blocking: có, tối đa cfg.timeout_ms + thời gian chuyển đổi (~20 ms)
 * Bộ nhớ  : ghi vào *out do người gọi cấp; driver không giữ con trỏ này
 * Thread  : không an toàn cho nhiều task cùng lúc trên cùng handle
 * Trả về  : ESP_OK
 *            ESP_ERR_INVALID_ARG    dev hoặc out là NULL
 *            ESP_ERR_TIMEOUT        thiết bị không phản hồi
 *            ESP_ERR_INVALID_CRC    dữ liệu hỏng
 *            ESP_ERR_INVALID_STATE  driver đang ở trạng thái ERROR, gọi *_recover() trước
 */
esp_err_t sht3x_read(sht3x_t *dev, sht3x_data_t *out);
```

Quy tắc:
- Một thiết bị = một handle opaque. Không biến static toàn cục giữ instance.
- Driver **nhận** bus/port từ ngoài, không tự `i2c_new_master_bus()` bên trong — vì bus dùng
  chung cho nhiều thiết bị và vòng đời do tầng trên quản.
- Hàm chuyển đổi raw → đơn vị kỹ thuật tách thành hàm thuần, không tham số handle:
  `float sht3x_raw_to_celsius(uint16_t raw);` — đây là phần test được trên host.
- Không đặt tham số nghiệp vụ (ngưỡng, chu kỳ đo) vào config của driver.

## 2. Timeout

- Mọi lời gọi bus có tham số timeout đều phải truyền giá trị hữu hạn. Cấm `portMAX_DELAY`.
- Giá trị lấy từ datasheet, không đặt bừa: thời gian chuyển đổi + biên an toàn (thường ×2).
  Ghi nguồn gốc con số vào comment.
- Chờ dài hơn một chu kỳ tick (10 ms) thì dùng `vTaskDelay`, không `esp_rom_delay_us`.
- Chờ rất dài (DS18B20 750 ms, e-paper vài giây): **tách API**

```c
esp_err_t ds18b20_start_conversion(ds18b20_t *d);           /* trả ngay */
esp_err_t ds18b20_conversion_ready(ds18b20_t *d, bool *rdy);
esp_err_t ds18b20_read(ds18b20_t *d, float *celsius);       /* ESP_ERR_NOT_FINISHED nếu chưa xong */
```

Tầng trên quyết định chờ thế nào. Driver không được ngủ thay người gọi.

- Vòng chờ cờ phần cứng luôn có mốc thoát theo thời gian thật, không theo số vòng lặp:

```c
int64_t deadline = esp_timer_get_time() + timeout_ms * 1000;
while (!hw_ready()) {
    if (esp_timer_get_time() > deadline) return ESP_ERR_TIMEOUT;
    vTaskDelay(pdMS_TO_TICKS(2));
}
```

## 3. Error handling

Dùng mã `esp_err_t` chuẩn, đừng tự định nghĩa mã mới nếu đã có mã phù hợp:

| Tình huống | Mã trả về |
|---|---|
| tham số NULL / ngoài dải | `ESP_ERR_INVALID_ARG` |
| thiết bị không ACK, không phản hồi | `ESP_ERR_TIMEOUT` |
| CRC / checksum sai | `ESP_ERR_INVALID_CRC` |
| đọc được nhưng giá trị ngoài dải vật lý | `ESP_ERR_INVALID_RESPONSE` |
| gọi khi chưa init hoặc đang ERROR | `ESP_ERR_INVALID_STATE` |
| chưa xong, gọi lại sau | `ESP_ERR_NOT_FINISHED` |
| chip ID sai khi init | `ESP_ERR_NOT_FOUND` |

Bắt buộc:
- Kiểm tra CRC/checksum nếu giao thức có. Không có CRC thì kiểm tra dải hợp lệ
  (nhiệt độ −40..85 °C, độ ẩm 0..100 %). Giá trị toàn `0x00` hoặc toàn `0xFF` gần như luôn
  là lỗi bus, không phải số đo.
- `ESP_ERROR_CHECK` chỉ dùng cho lỗi lập trình lúc init. Lỗi runtime phải trả lên.
- Không nuốt lỗi: mọi `esp_err_t` nhận về đều được kiểm tra hoặc trả tiếp.

## 4. Recovery

Thang leo, dừng ở nấc nào đủ:

1. **Retry ngắn** — 2–3 lần, có khoảng nghỉ nhỏ, chỉ cho lỗi thoáng qua (timeout, CRC).
   Số lần lấy từ config, không hard-code, không vô hạn.
2. **Reset mềm thiết bị** — lệnh soft-reset theo datasheet, rồi nạp lại cấu hình.
3. **Reset bus** — I2C treo: phát 9 xung clock rồi tạo lại bus. UART: `uart_flush_input`.
   TWAI: `twai_initiate_recovery` sau bus-off.
4. **Cắt nguồn thiết bị** — nếu board có chân điều khiển nguồn cho module đó.
5. **Trả lên trên** — chuyển sang state `ERROR` và trả lỗi. Tầng trên quyết định báo động,
   degrade, hay reset thiết bị.

Driver không được tự `esp_restart()`, không tự tạo task retry nền, không chặn task gọi
quá tổng timeout đã công bố trong header.

## 5. Trạng thái driver

```c
typedef enum {
    DRV_STATE_UNINIT = 0,
    DRV_STATE_READY,
    DRV_STATE_ERROR,       /* lỗi liên tiếp vượt ngưỡng, dữ liệu không tin được */
    DRV_STATE_RECOVERING,
} drv_state_t;
```

- Vào `ERROR` sau N lỗi liên tiếp (N trong config, mặc định 3), không phải sau một lỗi lẻ.
- Ở `ERROR`, hàm đọc trả `ESP_ERR_INVALID_STATE` ngay lập tức — **không** trả giá trị đo
  gần nhất như thể vẫn còn đúng. Muốn giá trị cũ thì tầng trên tự nhớ, kèm dấu thời gian.
- Kèm theo đếm để chẩn đoán: `ok_count`, `err_count`, `last_err`, `last_ok_us`.
  Cho truy vấn qua `*_get_stats()` — rất đáng giá khi thiết bị đã ra thực địa.

## 6. Logging

- Một `static const char *TAG = "sht3x";` mỗi file.
- `ESP_LOGE` khi vào ERROR hoặc hỏng không phục hồi được. `ESP_LOGW` khi retry/CRC lẻ.
  `ESP_LOGD` cho giá trị từng lần đọc. **Không** `ESP_LOGI` trong đường đọc lặp lại.
- Log lỗi phải kèm ngữ cảnh đủ để chẩn đoán: `ESP_LOGW(TAG, "read failed addr=0x%02x err=%s retry=%d", ...)`.
- Không log trong ISR. Không log trong vòng lặp nóng.
- Không log dữ liệu nhạy cảm (khoá, token) — xem `esp32-10-security`.

## 7. Test

Viết driver sao cho test được, kể cả trước khi có phần cứng:

- Hàm chuyển đổi/parse là hàm thuần → test trên host với vector từ datasheet.
- Parser giao thức nhận `(const uint8_t *buf, size_t len)` → test bằng frame dựng sẵn,
  gồm cả frame đứt đôi và frame sai CRC.
- Tách lời gọi bus qua một struct hàm nếu (và chỉ nếu) thật sự cần mock:

```c
typedef struct {
    esp_err_t (*write_read)(void *ctx, const uint8_t *tx, size_t txn, uint8_t *rx, size_t rxn);
    void *ctx;
} bus_io_t;
```
  Driver đơn giản, dùng một lần, không cần lớp này — đừng thêm cho "đẹp".
- Cung cấp `*_self_test()` đọc chip ID / thanh ghi hằng số để phân biệt "sai dây"
  và "sai code" ngay trên board.

Viết test thật → `esp32-08-testing`.
