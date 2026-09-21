/**
 * ============================================================================
 * AIoT_LIB - Ví Dụ 02: Điều Khiển Phần Cứng Linh Hoạt (Dynamic Pin HAL)
 * ============================================================================
 * Mô tả:
 * - Độc lập phần cứng 100%: Người dùng tự do chỉ định chân GPIO ở runtime
 * - Đăng ký nhãn thiết bị (Label) để Edge AI và Cloud AI hiểu rõ chức năng
 * - Các cách thao tác Relay: Hướng đối tượng (.on, .off, .toggle), gán trực tiếp (=), hoặc hàm
 * - Điều khiển còi Buzzer, đèn LED đơn, đèn LED RGB
 * - Đọc thông số cảm biến: Điện áp, ADC, nhiệt độ chip, dung lượng RAM trống
 * - Tự động tạo cấu hình phần cứng (Hardware Prompt) cho AI
 */

#include <Arduino.h>
#include <AIoT.h>

// Định nghĩa các chân GPIO tùy ý theo bo mạch thực tế của bạn
#define PIN_FAN_RELAY    14 // Rơ-le điều khiển quạt làm mát
#define PIN_PUMP_RELAY   15 // Rơ-le điều khiển máy bơm
#define PIN_LIGHT_RELAY  16 // Rơ-le chiếu sáng
#define PIN_BUZZER       4  // Còi buzzer cảnh báo
#define PIN_RGB_LED      48 // Đèn LED RGB (trên ESP32-S3)
#define PIN_POTENTIOMETER 6 // Cảm biến biến trở / áp suất (ADC)

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASS";

// Lắng nghe sự kiện điều khiển Quạt từ Cloud
Virtual_WRITE(fan)
{
    int state = param.getInt();
    AIoT_Device.Relay(PIN_FAN_RELAY, state ? true : false);
    AIoT.writeControl("fan", state);
    Serial.printf("[CONTROL] Quat da chuyen sang: %s\n", state ? "BAT" : "TAT");
}

// Lắng nghe sự kiện điều khiển Bơm từ Cloud
Virtual_WRITE(pump)
{
    int state = param.getInt();
    AIoT_Device.Relay(PIN_PUMP_RELAY, state ? true : false);
    AIoT.writeControl("pump", state);
    Serial.printf("[CONTROL] May bom da chuyen sang: %s\n", state ? "BAT" : "TAT");
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n==================================================");
    Serial.println("   AIoT_LIB: Dynamic Hardware Device Control Demo");
    Serial.println("==================================================");

    // 1. ĐĂNG KÝ CÁC CHÂN THIẾT BỊ ĐỘNG KÈM NHÃN TÊN CHO AI
    AIoT_Device.Relay(PIN_FAN_RELAY, "Quat hut cong nghiep");
    AIoT_Device.Relay(PIN_PUMP_RELAY, "May bom lam mat");
    AIoT_Device.Relay(PIN_LIGHT_RELAY, "Den chieu sang xuong");

    // 2. CÀI ĐẶT CHÂN CÒI VÀ ĐÈN
    AIoT_Device.setBuzzerPin(PIN_BUZZER);
    AIoT_Device.setRgbPin(PIN_RGB_LED);

    // 3. MINH HỌA CÁC CÁCH ĐIỀU KHIỂN HƯỚNG ĐỐI TƯỢNG (OOP)
    Serial.println("\n[DEMO 1] Dieu khien Relay huong doi tuong (OOP):");
    AIoT_Device.Relay(PIN_FAN_RELAY).on();
    Serial.printf("- Trang thai Quat: %s\n", AIoT_Device.Relay(PIN_FAN_RELAY) ? "ON" : "OFF");
    delay(300);

    AIoT_Device.Relay(PIN_FAN_RELAY).off();
    Serial.printf("- Trang thai Quat sau off(): %s\n", AIoT_Device.Relay(PIN_FAN_RELAY) ? "ON" : "OFF");
    delay(300);

    // Toán tử gán trực tiếp:
    AIoT_Device.Relay(PIN_PUMP_RELAY) = true;
    Serial.printf("- May bom duoc bat qua toan tu gan (= true): %s\n", AIoT_Device.Relay(PIN_PUMP_RELAY) ? "ON" : "OFF");
    delay(300);
    AIoT_Device.Relay(PIN_PUMP_RELAY) = false;

    // 4. MINH HỌA CÒI BUZZER & LED RGB
    Serial.println("\n[DEMO 2] Phat coi bip va doi mau RGB:");
    AIoT_Device.beep(80);           // Bíp còi 80ms
    AIoT_Device.rgb(0, 255, 0);     // Đèn RGB màu xanh lá
    delay(300);
    AIoT_Device.rgb(0, 0, 0);       // Tắt đèn RGB

    // 5. TỰ ĐỘNG SINH PROMPT PHẦN CỨNG CHO AI (GEMINI / CLOUD)
    Serial.println("\n[DEMO 3] Thong tin phan cung tu sinh cho AI (Hardware Prompt):");
    Serial.println(AIoT_Device.getHardwarePrompt());

    // 6. KHỞI TẠO KẾT NỐI CLOUD
    AIoT.begin(WIFI_SSID, WIFI_PASS);
}

void loop()
{
    AIoT.run();

    // Đọc cảm biến định kỳ mỗi 4 giây
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 4000)
    {
        lastCheck = millis();

        float voltage = AIoT_Device.readVoltage(PIN_POTENTIOMETER);
        float chipTemp = AIoT_Device.readChipTemp();
        uint32_t ram = AIoT_Device.readFreeRam();

        Serial.printf("[STATUS] Volts: %.2fV | ChipTemp: %.2f*C | FreeRAM: %u B | Quat: %d | Bom: %d\n",
                      voltage, chipTemp, ram,
                      (int)AIoT_Device.Relay(PIN_FAN_RELAY),
                      (int)AIoT_Device.Relay(PIN_PUMP_RELAY));

        if (AIoT.CheckConnect())
        {
            AIoT.updateTelemetry("voltage", voltage);
            AIoT.updateTelemetry("chip_temp", chipTemp);
            AIoT.updateTelemetry("fan", (int)AIoT_Device.Relay(PIN_FAN_RELAY));
            AIoT.updateTelemetry("pump", (int)AIoT_Device.Relay(PIN_PUMP_RELAY));
            AIoT.sendTelemetry();
        }
    }
}
