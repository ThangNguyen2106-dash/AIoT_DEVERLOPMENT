/**
 * ==============================================================================
 * VÍ DỤ 01: BASIC IOT & TELEMETRY STREAM
 * ==============================================================================
 * Hướng dẫn sử dụng tầng cơ sở của AIoT Platform:
 * 1. Khởi tạo kết nối mạng và MQTT
 * 2. Đọc cảm biến và điều khiển chân GPIO chuẩn
 * 3. Gửi dữ liệu Telemetry thời gian thực lên Cloud / MQTT Dashboard
 * ==============================================================================
 */

#define DEBUG_COLOR
#include <Arduino.h>
#include <AIoT.h>

// Định nghĩa chân GPIO kết nối phần cứng
#define PIN_RELAY_LIGHT 12  // Rơ-le 1: Đèn chiếu sáng
#define PIN_RELAY_PUMP  13  // Rơ-le 2: Máy bơm nước

bool relayLightState = false;

void setup()
{
    Serial.begin(115200);

    // 1. Cấu hình chân GPIO phần cứng trực tiếp
    pinMode(PIN_RELAY_LIGHT, OUTPUT);
    pinMode(PIN_RELAY_PUMP, OUTPUT);

    // Khởi tạo trạng thái ban đầu
    digitalWrite(PIN_RELAY_LIGHT, HIGH);
    relayLightState = true;
    digitalWrite(PIN_RELAY_PUMP, LOW);

    Serial.println(F("[SYSTEM] Basic IoT Initialized!"));
}

void loop()
{
    // Duy trì các tác vụ nền của giao thức AIoT (WiFi, MQTT)
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
        AIoT.updateTelemetry("relay_light_state", relayLightState);

        Serial.printf("[TELEMETRY] Temp: %.1f C | Light: %d | Relay: %s\n",
                      ambientTemp, lightIntensity,
                      relayLightState ? "ON" : "OFF");
    }

    delay(10);
}
