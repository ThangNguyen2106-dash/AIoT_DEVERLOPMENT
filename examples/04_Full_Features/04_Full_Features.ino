#include <Arduino.h>

#ifndef DEBUG_COLOR
#define DEBUG_COLOR
#endif

#ifndef BUTTON_CONFIG
#define BUTTON_CONFIG
#endif

#include <AIoT.h>

// ======================================================
// 1. THÔNG TIN KẾT NỐI WIFI
// ======================================================
const char *WIFI_SSID = "";
const char *WIFI_PASS = "";

const char *MQTT_SSID = "";
const char *MQTT_PASS = "";

// Chân LED báo trạng thái hoặc Relay
#define STATUS_LED_PIN 2

// ======================================================
// 2. NHẬN LỆNH ĐIỀU KHIỂN TỪ WEB UI / AI (MQTT)
// ======================================================
// Bắt sự kiện khi nhận lệnh điều khiển "relay1" hoặc "led"
Virtual_WRITE(relay1)
{
    int state = param.getInt();
    digitalWrite(STATUS_LED_PIN, state ? HIGH : LOW);
    Serial.printf("[MQTT RECV] Relay/LED: %d\n", state);

    // Phản hồi lại trạng thái xác nhận về Topic control
    AIoT.writeControl("relay1", state);
}

// ======================================================
// 3. ĐỌC NHIỆT ĐỘ CHIP & THÔNG SỐ HỆ THỐNG GỬI LÊN BROKER
// ======================================================
void sendChipTelemetry()
{
    if (!AIoT.CheckConnect())
    {
        return; // Bỏ qua nếu chưa kết nối WiFi
    }
    if (!serverMQTT.check_connect())
    {
        return; // Bỏ qua nếu chưa kết nối MQTT
    }

    // 1. Đọc cảm biến nhiệt độ bên trong chip ESP32 (Đơn vị: °C)
    float chipTemp = temperatureRead();

    // 2. Đọc thêm các thông số hệ thống hữu ích
    uint32_t freeRam = ESP.getFreeHeap();      // Dung lượng RAM còn trống (bytes)
    int8_t wifiRssi = WiFi.RSSI();             // Cường độ sóng WiFi (dBm)
    unsigned long uptimeSec = millis() / 1000; // Thời gian chạy (giây)

    Serial.println("\n--- [TELEMETRY UPDATE] ---");
    Serial.printf("[SYSTEM] Nhiet do chip ESP32: %.2f *C\n", chipTemp);
    Serial.printf("[SYSTEM] RAM trong (Free Heap): %u bytes\n", freeRam);
    Serial.printf("[SYSTEM] Tin hieu WiFi (RSSI): %d dBm\n", wifiRssi);
    Serial.printf("[SYSTEM] Thoi gian hoat dong: %lu s\n", uptimeSec);
    Serial.println("--------------------------");

    // 3. Đóng gói & gửi lên Topic: device/<MAC>/telemetry
    AIoT.writeTelemetry("chip_temp", chipTemp);
    AIoT.writeTelemetry("free_ram", (int)freeRam);
    AIoT.writeTelemetry("wifi_rssi", wifiRssi);
    AIoT.writeTelemetry("uptime", (int)uptimeSec);
}

// ======================================================
// 4. SETUP & LOOP
// ======================================================
void setup()
{
    Serial.begin(115200);
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    // Khởi tạo và kết nối thư viện với WiFi & HiveMQ Broker
    AIoT.begin(WIFI_SSID, WIFI_PASS, MQTT_SSID, MQTT_PASS);

    // Hẹn giờ tự động đọc và gửi nhiệt độ mỗi 3 giây (3000ms)
    AIoT.addTimeEvent(3000, sendChipTelemetry);
}

void loop()
{
    // Duy trì toàn bộ hoạt động của thư viện
    AIoT.run();
}