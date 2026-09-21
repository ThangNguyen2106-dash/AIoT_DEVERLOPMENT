/**
 * ============================================================================
 * AIoT_LIB - Ví Dụ 01: Cơ Bản Về IoT & Đồng Bộ Dữ Liệu Lên Cloud
 * ============================================================================
 * Mô tả:
 * - Kết nối WiFi tự động (hoặc qua Smart Captive Portal Web AP: 192.168.21.6)
 * - Độc lập phần cứng: Định nghĩa chân Relay động qua AIoT_Device.Relay(pin, "Tên")
 * - Lắng nghe sự kiện điều khiển từ xa qua macro Virtual_WRITE
 * - Đóng gói và gửi Telemetry (nhiệt độ chip, RAM, RSSI, uptime) lên HiveMQ Cloud TLS
 */

#include <Arduino.h>
#include <AIoT.h>

// Thông tin mạng WiFi (để trống nếu muốn cấu hình qua Web AP 192.168.21.6)
const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASS";

// Thông tin tài khoản HiveMQ Cloud TLS (Port 8883)
const char *MQTT_USER = "IoT_TEST";
const char *MQTT_PASS = "mt21062005";

// Định nghĩa chân Relay linh hoạt theo bo mạch của bạn
#define PIN_RELAY_MAIN 14

// Bắt sự kiện điều khiển từ xa từ App/Dashboard
Virtual_WRITE(relay1)
{
    int state = param.getInt();
    // Điều khiển Relay chân động
    AIoT_Device.Relay(PIN_RELAY_MAIN, state ? true : false);
    Serial.printf("[MQTT RECV] Dieu khien Relay: %d\n", state);

    // Xác nhận phản hồi trạng thái ngược lại Server
    AIoT.writeControl("relay1", state);
}

// Gửi Telemetry định kỳ mỗi 3 giây
void sendTelemetryData()
{
    if (!AIoT.CheckConnect())
        return;

    float chipTemp = AIoT_Device.readChipTemp();
    uint32_t freeRam = AIoT_Device.readFreeRam();
    int8_t wifiRssi = WiFi.RSSI();
    unsigned long uptime = millis() / 1000;

    Serial.printf("[TELEMETRY] Temp: %.2f *C | RAM: %u B | RSSI: %d dBm | Uptime: %lu s\n",
                  chipTemp, freeRam, wifiRssi, uptime);

    AIoT.updateTelemetry("chip_temp", chipTemp);
    AIoT.updateTelemetry("free_ram", (int)freeRam);
    AIoT.updateTelemetry("wifi_rssi", wifiRssi);
    AIoT.updateTelemetry("uptime", (int)uptime);
    AIoT.updateTelemetry("relay_state", (int)AIoT_Device.Relay(PIN_RELAY_MAIN));

    // Đóng gói JSON và gửi lên Broker trong 1 gói tin duy nhất
    AIoT.sendTelemetry();
}

void setup()
{
    Serial.begin(115200);

    // Đăng ký chân Relay và nhãn nhận diện thiết bị
    AIoT_Device.Relay(PIN_RELAY_MAIN, "Tai cong nghiep 1");

    // Khởi tạo kết nối mạng và HiveMQ Cloud TLS
    AIoT.begin(WIFI_SSID, WIFI_PASS, MQTT_USER, MQTT_PASS);

    // Hẹn giờ gửi dữ liệu telemetry mỗi 3000ms
    AIoT.addTimeEvent(3000, sendTelemetryData);

    Serial.println("[SYSTEM] He thong AIoT Basic khoi dong thanh cong.");
}

void loop()
{
    // BẮT BUỘC: Giữ kết nối MQTT, Web Captive Portal và xử lý Event Timer
    AIoT.run();
}
