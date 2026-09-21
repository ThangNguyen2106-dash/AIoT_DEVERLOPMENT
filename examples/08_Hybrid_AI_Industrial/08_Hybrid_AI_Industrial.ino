/**
 * ============================================================================
 * AIoT_LIB - Ví Dụ 08: Điện Toán Lai Đa Tầng (Hybrid AI Industrial Architecture)
 * ============================================================================
 * Chức năng:
 * - Tầng 1 (Edge AI): Xử lý rung chấn thời gian thực, ngắt rơ-le trong 0.1ms
 *                     bảo vệ thiết bị tại chỗ khi gặp sự cố mà không cần Internet.
 * - Tầng 2 (AIoT Core): Đóng gói chỉ số thống kê gửi lên HiveMQ Cloud TLS 8883.
 * - Tầng 3 (Cloud AI): Tự động kích hoạt Google Gemini chẩn đoán nguyên nhân gốc
 *                      khi phát hiện bất thường.
 * ============================================================================
 */

#include <Arduino.h>
#include <AIoT.h>

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASS";
const char *MQTT_USER = "IoT_TEST";
const char *MQTT_PASS = "mt21062005";
const char *GEMINI_KEY = "YOUR_GEMINI_API_KEY";

#define PIN_MOTOR_RELAY 14 // Rơ-le động cơ chính
#define PIN_FAN_RELAY   15 // Rơ-le quạt làm mát
#define PIN_BUZZER      4  // Còi báo động

HybridAIEngine hybridAI;
unsigned long lastTelemetry = 0;
unsigned long lastDiagnosis = 0;

// Giả lập tín hiệu cảm biến rung động động cơ
float readMotorVibration()
{
    static unsigned long count = 0;
    count++;
    float noise = (float)(random(-5, 5)) / 10.0f;
    float val = 4.5f + noise;
    if (count % 30 == 0) // Tạo đột biến rung giật mạnh mỗi 30 chu kỳ
        val = 19.5f;
    return val;
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== AIoT_LIB: Vi Du 08 - He Thong Dien Toan Lai (Hybrid AI) ===");

    // 1. Đăng ký phần cứng
    AIoT_Device.Relay(PIN_MOTOR_RELAY, "Động cơ chính");
    AIoT_Device.Relay(PIN_FAN_RELAY, "Quạt giải nhiệt");
    AIoT_Device.setBuzzerPin(PIN_BUZZER);
    AIoT_Device.Relay(PIN_MOTOR_RELAY).on(); // Mặc định bật động cơ

    // 2. Cài đặt Hybrid AI
    hybridAI.setGeminiApiKey(GEMINI_KEY);
    hybridAI.begin(3.0f, 20);           // Ngưỡng 3.0 sigma, 20 mẫu hiệu chuẩn
    hybridAI.teachBaseline(4.5f, 0.5f); // Thiết lập giá trị tiêu chuẩn

    // 3. Khởi động mạng và kết nối Cloud
    AIoT.begin(WIFI_SSID, WIFI_PASS, MQTT_USER, MQTT_PASS);
}

void loop()
{
    AIoT.run();

    // 1. TẦNG BIÊN (EDGE AI): Suy luận tại chỗ (< 1ms)
    float sample = readMotorVibration();
    EdgeAI::InferenceResult res = hybridAI.process(sample);

    // 2. PHẢN XẠ AN TOÀN TẠI CHỖ (LOCAL FAIL-SAFE)
    if (res.isEmergency)
    {
        Serial.printf("[EDGE_AI CRITICAL] Phat hien su co! Gia tri: %.2f mm/s\n", sample);
        AIoT_Device.Relay(PIN_MOTOR_RELAY).off(); // Ngắt động cơ tức khắc
        AIoT_Device.Relay(PIN_FAN_RELAY).on();    // Bật quạt giải nhiệt
        AIoT_Device.beep(150);

        // 3. TẦNG ĐÁM MÂY (CLOUD AI): Đánh thức Gemini chẩn đoán nguyên nhân
        if (AIoT.CheckConnect() && (millis() - lastDiagnosis > 30000))
        {
            lastDiagnosis = millis();
            Serial.println("[CLOUD_AI] Yeu cau Gemini chan doan nguyen nhan...");

            String prompt = "Dong co bi rung dot bien: " + String(sample) + " mm/s. Hay chan doan nguyen nhan hu hong.";
            String advice = hybridAI.gemini.ask(prompt, AIoT_Device.getHardwarePrompt());

            Serial.println("\n[GEMINI ADVICE]:\n" + advice);
            AIoT_Device.executeCommand(advice);
        }
    }

    // 4. ĐỒNG BỘ TELEMETRY LÊN CLOUD ĐỊNH KỲ (MỖI 3 GIÂY)
    if (millis() - lastTelemetry >= 3000)
    {
        lastTelemetry = millis();
        AIoT.updateTelemetry("vibration", sample);
        AIoT.updateTelemetry("motor", (int)AIoT_Device.Relay(PIN_MOTOR_RELAY));
        AIoT.updateTelemetry("fan", (int)AIoT_Device.Relay(PIN_FAN_RELAY));
        AIoT.sendTelemetry();
    }

    delay(100);
}
