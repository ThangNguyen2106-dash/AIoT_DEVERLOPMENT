#ifndef BOARD_ESP32_S3_KIT_H
#define BOARD_ESP32_S3_KIT_H

#define BOARD_NAME "ESP32-S3 AIoT DevKit"

// LED & Buzzer
#ifndef PIN_STATUS_LED
#define PIN_STATUS_LED   2
#endif
#ifndef PIN_RGB_LED
#define PIN_RGB_LED      48 // WS2812 RGB LED trên ESP32-S3
#endif
#ifndef PIN_BUZZER
#define PIN_BUZZER       21
#endif

// Relays / Actuators (Chân mặc định chuẩn cho Kit ESP32-S3 AIoT)
#ifndef PIN_RELAY_1
#define PIN_RELAY_1      14 // Relay 1 (Tải chính / Quạt)
#endif
#ifndef PIN_RELAY_2
#define PIN_RELAY_2      15 // Relay 2 (Tải phụ / Máy bơm / Van)
#endif
#ifndef PIN_RELAY_3
#define PIN_RELAY_3      -1
#endif
#ifndef PIN_RELAY_4
#define PIN_RELAY_4      -1
#endif

// Buttons
#ifndef PIN_BUTTON_USER
#define PIN_BUTTON_USER  0
#endif
#ifndef PIN_BUTTON_CONFIG
#define PIN_BUTTON_CONFIG 0
#endif

// Sensors / Analog (ADC1 trên ESP32-S3)
#ifndef PIN_ANALOG_1
#define PIN_ANALOG_1     6  // Cảm biến biến trở (ADC1_CH5)
#endif
#ifndef PIN_ANALOG_2
#define PIN_ANALOG_2     1
#endif
#ifndef PIN_ANALOG_3
#define PIN_ANALOG_3     2
#endif
#ifndef PIN_ANALOG_4
#define PIN_ANALOG_4     3
#endif

// I2C Bus
#define PIN_I2C_SDA      8
#define PIN_I2C_SCL      9

// I2S Microphone (Cho Voice / Sound AI)
#define PIN_I2S_MIC_SCK  41
#define PIN_I2S_MIC_WS   42
#define PIN_I2S_MIC_SD   40

// UART Modbus RS485
#define PIN_RS485_RX     17
#define PIN_RS485_TX     18
#define PIN_RS485_DE_RE  16

#endif /* BOARD_ESP32_S3_KIT_H */

