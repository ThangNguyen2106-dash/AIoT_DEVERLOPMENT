/**
 * ============================================================================
 * AIoT_LIB - Ví Dụ 01: Điều Khiển IoT Cơ Bản (Basic IoT & Remote Control)
 * ============================================================================
 * Chức năng:
 * - Kết nối WiFi & Broker HiveMQ Cloud TLS (Cổng 8883)
 * - Lắng nghe sự kiện điều khiển từ xa từ Web Dashboard / App qua macro Virtual_WRITE
 * - Bật / Tắt rơ-le và gửi phản hồi xác nhận trạng thái về Cloud
 * ============================================================================
 */

#include <Arduino.h>
#include <AIoT.h>

// Thông tin kết nối WiFi
const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASS";

// Tài khoản HiveMQ Cloud TLS (Port 8883)
const char *MQTT_USER = "IoT_TEST";
const char *MQTT_PASS = "mt21062005";

// Chân GPIO điều khiển Rơ-le (tùy chỉnh theo bo mạch thực tế của bạn)
#define PIN_RELAY1 14

// Lắng nghe sự kiện điều khiển từ xa với định danh "relay1"
Virtual_WRITE(relay1)
{
    int state = param.getInt(); // Nhận giá trị 1 (BẬT) hoặc 0 (TẮT)
    AIoT_Device.Relay(PIN_RELAY1, state ? true : false);

    Serial.printf("[MQTT RECV] Relay 1 da chuyen sang: %s\n", state ? "BAT" : "TAT");

    // Xác nhận trạng thái đã thực thi thành công về Server
    AIoT.writeControl("relay1", state);
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== AIoT_LIB: Vi Du 01 - Dieu Khien IoT Co Ban ===");

    // Đăng ký chân Rơ-le với nhãn hiển thị
    AIoT_Device.Relay(PIN_RELAY1, "Rơ-le 1");

    // Khởi động kết nối WiFi và HiveMQ Cloud
    AIoT.begin(WIFI_SSID, WIFI_PASS, MQTT_USER, MQTT_PASS);
}

void loop()
{
    // Duy trì kết nối mạng và xử lý sự kiện (bắt buộc gọi trong loop)
    AIoT.run();
}
