# ADC

## Bẫy quan trọng

- **ADC2 không dùng được khi Wi-Fi bật.** Luôn ưu tiên ADC1 cho cảm biến analog.
- ADC của ESP32 phi tuyến rõ ở hai đầu dải. Chỉ tin cậy khoảng giữa (khoảng 150 mV đến 2450 mV
  với attenuation 12 dB).
- Cần giá trị điện áp thật thì bắt buộc hiệu chuẩn bằng `adc_cali_*`, không tự nhân tỉ lệ thô.
- Đo điện áp pin qua cầu chia áp: trở quá lớn (trên 100k) sẽ sai do trở kháng vào ADC.
  Thêm tụ 100 nF ở chân ADC, hoặc dùng op-amp đệm.

## Oneshot (đo thưa)

```c
adc_oneshot_unit_handle_t adc1;
adc_oneshot_unit_init_cfg_t init = { .unit_id = ADC_UNIT_1 };
ESP_ERROR_CHECK(adc_oneshot_new_unit(&init, &adc1));

adc_oneshot_chan_cfg_t ch = {
    .atten = ADC_ATTEN_DB_12,
    .bitwidth = ADC_BITWIDTH_DEFAULT,
};
ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1, ADC_CHANNEL_0, &ch));

int raw;
ESP_ERROR_CHECK(adc_oneshot_read(adc1, ADC_CHANNEL_0, &raw));
```

Lọc nhiễu: lấy **trung vị** của 5–9 mẫu tốt hơn trung bình, vì loại được gai nhiễu đơn lẻ.
Tín hiệu biến thiên chậm thì thêm bộ lọc IIR một bậc.

## Continuous (lấy mẫu tốc độ cao)

Dùng `adc_continuous_*` với DMA khi cần trên vài kHz. Đăng ký callback chuyển đổi xong,
đọc theo frame, không đọc từng mẫu. Callback chạy trong ngữ cảnh ISR: chỉ đẩy dữ liệu
vào queue, không xử lý tại chỗ.
