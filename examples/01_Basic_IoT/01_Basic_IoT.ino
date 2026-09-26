/**
 * ==============================================================================
 * VÍ DỤ 01: BASIC IOT & DEVICE HAL
 * ==============================================================================
 * Hướng dẫn sử dụng tầng cơ sở của AIoT Platform:
 * 1. Khởi tạo Hardware Abstraction Layer (HAL) qua AIoT.device
 * 2. Gắn và điều khiển Rơ-le (Relay) an toàn
 * 3. Gửi dữ liệu Telemetry lên Cloud / MQTT Dashboard
 * ==============================================================================
 */

#define DEBUG_COLOR
#include <Arduino.h>
#include <AIoT.h>

// Định nghĩa chân GPIO kết nối phần cứng
#define PIN_RELAY_LIGHT 12  // Rơ-le 1: Đèn chiếu sáng
#define PIN_RELAY_PUMP  13  // Rơ-le 2: Máy bơm nước

void setup()
{
    Serial.begin(115200);

    // 1. Khởi tạo tầng phần cứng HAL
    AIoT.device.begin();

    // 2. Gắn các thiết bị Rơ-le vào HAL
    AIoT.device.attachRelay(PIN_RELAY_LIGHT, "Main_Light");
    AIoT.device.attachRelay(PIN_RELAY_PUMP, "Water_Pump");

    // Thử nghiệm bật rơ-le đèn khi khởi động
    AIoT.device.Relay(PIN_RELAY_LIGHT).on();

    Serial.println(F("[SYSTEM] Basic IoT HAL Initialized!"));
}

void loop()
{
    // Duy trì các tác vụ nền của giao thức AIoT (WiFi, MQTT, HAL)
    AIoT.run();

    static unsigned long lastSend = 0;
    if (millis() - lastSend > 2000) // Chu kỳ gửi 2 giây
    {
        lastSend = millis();

        // Đọc giá trị giả lập hoặc từ cảm biến
        float ambientTemp = 28.5f + (float)random(-10, 10) / 10.0f;
        int lightIntensity = analogRead(34);

        // Bắn telemetry lên Dashboard
        AIoT.updateTelemetry("temp_c", ambientTemp);
        AIoT.updateTelemetry("light_adc", lightIntensity);
        AIoT.updateTelemetry("relay_light_state", AIoT.device.Relay(PIN_RELAY_LIGHT).getState());

        Serial.printf("[TELEMETRY] Temp: %.1f C | Light: %d | Relay: %s\n",
                      ambientTemp, lightIntensity,
                      AIoT.device.Relay(PIN_RELAY_LIGHT).getState() ? "ON" : "OFF");
    }

    delay(10);
}
