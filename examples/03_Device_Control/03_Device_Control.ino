/**
 * ============================================================================
 * AIoT_LIB - Ví Dụ 03: Điều Khiển Phần Cứng Linh Hoạt (Hardware HAL)
 * ============================================================================
 * Chức năng:
 * - Độc lập phần cứng 100%: Người dùng tự do chỉ định chân GPIO lúc runtime
 * - Thao tác Relay theo phong cách hướng đối tượng: .on(), .off(), .toggle(), hoặc toán tử gán (=)
 * - Điều khiển còi Buzzer phát tiếng bíp và đổi màu đèn LED RGB
 * - Tự động sinh chuỗi mô tả phần cứng (Hardware Prompt) chuẩn hóa cho AI
 * ============================================================================
 */

#include <Arduino.h>
#include <AIoT.h>

// Định nghĩa các chân GPIO theo thiết kế thực tế của bạn
#define PIN_FAN_RELAY   14 // Rơ-le quạt làm mát
#define PIN_PUMP_RELAY  15 // Rơ-le máy bơm
#define PIN_BUZZER      4  // Còi buzzer báo động
#define PIN_RGB_LED     48 // Đèn LED RGB onboard (ESP32-S3)

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== AIoT_LIB: Vi Du 03 - Dieu Khien Phan Cung Linh Hoat (HAL) ===");

    // 1. ĐĂNG KÝ CHÂN THIẾT BỊ ĐỘNG KÈM NHÃN TÊN
    AIoT_Device.Relay(PIN_FAN_RELAY, "Quạt thông gió");
    AIoT_Device.Relay(PIN_PUMP_RELAY, "Máy bơm nước");
    AIoT_Device.setBuzzerPin(PIN_BUZZER);
    AIoT_Device.setRgbPin(PIN_RGB_LED);

    // 2. THAO TÁC RƠ-LE HƯỚNG ĐỐI TƯỢNG (OOP)
    Serial.println("\n[1] Thao tac Relay:");
    AIoT_Device.Relay(PIN_FAN_RELAY).on();
    Serial.printf("- Trang thai Quat: %s\n", AIoT_Device.Relay(PIN_FAN_RELAY) ? "ON" : "OFF");
    delay(500);

    AIoT_Device.Relay(PIN_FAN_RELAY).off();
    Serial.printf("- Trang thai Quat sau off(): %s\n", AIoT_Device.Relay(PIN_FAN_RELAY) ? "ON" : "OFF");
    delay(500);

    // Sử dụng toán tử gán trực tiếp:
    AIoT_Device.Relay(PIN_PUMP_RELAY) = true;
    Serial.printf("- May bom bat qua toan tu gan (= true): %s\n", AIoT_Device.Relay(PIN_PUMP_RELAY) ? "ON" : "OFF");
    delay(500);
    AIoT_Device.Relay(PIN_PUMP_RELAY) = false;

    // 3. ĐIỀU KHIỂN CÒI BUZZER & LED RGB
    Serial.println("\n[2] Phat coi bip va doi mau LED RGB:");
    AIoT_Device.beep(100);          // Kêu bíp 100ms
    AIoT_Device.rgb(0, 255, 0);    // Màu xanh lá (Hoạt động tốt)
    delay(500);
    AIoT_Device.rgb(0, 0, 0);      // Tắt LED RGB

    // 4. TỰ ĐỘNG SINH PROMPT PHẦN CỨNG CHO AI (GEMINI)
    Serial.println("\n[3] Mo ta phan cung tu sinh cho AI (Hardware Prompt):");
    Serial.println(AIoT_Device.getHardwarePrompt());
}

void loop()
{
    // Đảo trạng thái rơ-le mỗi 3 giây để kiểm tra
    AIoT_Device.Relay(PIN_FAN_RELAY).toggle();
    Serial.printf("[LOOP] Toggle Quat: %s\n", AIoT_Device.Relay(PIN_FAN_RELAY) ? "ON" : "OFF");
    delay(3000);
}

