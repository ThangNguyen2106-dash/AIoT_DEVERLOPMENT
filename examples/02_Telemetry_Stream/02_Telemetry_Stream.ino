/**
 * ============================================================================
 * AIoT_LIB - Ví Dụ 02: Phát Dòng Dữ Liệu Đo Đạc (Telemetry Stream)
 * ============================================================================
 * Chức năng:
 * - Thu thập thông số vận hành: Nhiệt độ chip (°C), dung lượng RAM trống (bytes),
 *   cường độ sóng WiFi RSSI (dBm), thời gian hoạt động (uptime).
 * - Đóng gói nhiều trường thông số vào MỘT gói tin JSON duy nhất để tiết kiệm băng thông.
 * - Gửi định kỳ lên HiveMQ Cloud qua phương thức AIoT.sendTelemetry().
 * ============================================================================
 */

#include <Arduino.h>
#include <AIoT.h>

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASS";

const char *MQTT_USER = "IoT_TEST";
const char *MQTT_PASS = "mt21062005";

// Hàm thu thập và đóng gói dữ liệu Telemetry
void sendSystemTelemetry()
{
    if (!AIoT.CheckConnect())
        return;

    float chipTemp = AIoT_Device.readChipTemp(); // Nhiệt độ cảm biến tích hợp trong chip
    uint32_t freeRam = AIoT_Device.readFreeRam(); // RAM còn trống (Heap)
    int8_t wifiRssi = WiFi.RSSI();                // Cường độ sóng WiFi
    unsigned long uptime = millis() / 1000;       // Thời gian hoạt động (giây)

    Serial.printf("[TELEMETRY] Temp: %.2f *C | RAM: %u B | RSSI: %d dBm | Uptime: %lu s\n",
                  chipTemp, freeRam, wifiRssi, uptime);

    // Nạp các trường dữ liệu vào bộ đệm Telemetry
    AIoT.updateTelemetry("chip_temp", chipTemp);
    AIoT.updateTelemetry("free_ram", (int)freeRam);
    AIoT.updateTelemetry("wifi_rssi", wifiRssi);
    AIoT.updateTelemetry("uptime", (int)uptime);

    // Đóng gói JSON và gửi toàn bộ lên Cloud trong một gói tin duy nhất
    AIoT.sendTelemetry();
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== AIoT_LIB: Vi Du 02 - Phat Dong Telemetry ===");

    // Khởi tạo kết nối mạng và Cloud
    AIoT.begin(WIFI_SSID, WIFI_PASS, MQTT_USER, MQTT_PASS);

    // Đăng ký bộ hẹn giờ tự động gửi dữ liệu mỗi 3000ms (3 giây)
    AIoT.addTimeEvent(3000, sendSystemTelemetry);
}

void loop()
{
    AIoT.run();
}
