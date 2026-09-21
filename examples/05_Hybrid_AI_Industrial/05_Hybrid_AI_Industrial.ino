/**
 * ============================================================================
 * AIoT_LIB - Ví Dụ 05: Toàn Diện Hybrid AIoT (Edge AI + HiveMQ + Gemini Cloud)
 * ============================================================================
 * Mô tả:
 * - Tầng 1 (Edge AI): Xử lý rung động / nhiệt độ theo thời gian thực (< 1ms),
 *                     tự động ngắt Relay bảo vệ máy khi phát hiện sự cố nguy cấp.
 * - Tầng 2 (AIoT Core): Đóng gói chỉ số thống kê (Mean, RMS, Z-Score) gửi lên
 *                       HiveMQ Cloud TLS 8883 định kỳ qua 1 gói tin JSON.
 * - Tầng 3 (Cloud AI): Tự động kích hoạt Google Gemini phân tích nguyên nhân gốc
 *                      khi phát hiện bất thường và nhận lệnh điều khiển từ xa.
 */

#include <Arduino.h>
#include <AIoT.h>

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASS";
const char *MQTT_USER = "IoT_TEST";
const char *MQTT_PASS = "mt21062005";
const char *GEMINI_KEY = "YOUR_GEMINI_API_KEY";

// Cấu hình chân phần cứng linh hoạt
#define PIN_MOTOR_RELAY 14 // Relay động cơ chính
#define PIN_COOLING_FAN 15 // Relay quạt giải nhiệt
#define PIN_BUZZER      4  // Còi báo động sự cố
#define PIN_SENSOR_ADC  6  // Chân ADC cảm biến rung động / nhiệt độ

HybridAIEngine hybridAI;
unsigned long lastTelemetryMs = 0;
unsigned long lastCloudDiagnosisMs = 0;

// Lắng nghe lệnh can thiệp khẩn từ xa của kỹ sư vận hành qua App/Dashboard
Virtual_WRITE(motor_control)
{
    int cmd = param.getInt();
    AIoT_Device.Relay(PIN_MOTOR_RELAY, cmd ? true : false);
    AIoT.writeControl("motor_control", cmd);
    Serial.printf("[REMOTE CMD] Trang thai Dong co: %s\n", cmd ? "BAT" : "TAT");
}

Virtual_WRITE(fan_control)
{
    int cmd = param.getInt();
    AIoT_Device.Relay(PIN_COOLING_FAN, cmd ? true : false);
    AIoT.writeControl("fan_control", cmd);
    Serial.printf("[REMOTE CMD] Trang thai Quat: %s\n", cmd ? "BAT" : "TAT");
}

// Giả lập đọc cảm biến rung động động cơ công nghiệp
float readMotorSensor()
{
    static unsigned long tick = 0;
    tick++;

    // Tín hiệu rung bình thường quanh 5.0 mm/s RMS
    float noise = (float)(random(-5, 5)) / 10.0f;
    float val = 5.0f + noise;

    // Giả lập rung giật mạnh do mòn bạc đạn mỗi 40 chu kỳ
    if (tick % 40 == 0)
    {
        val = 18.5f;
    }
    return val;
}

// Gửi toàn bộ telemetry phân tích lên HiveMQ Cloud
void sendHybridTelemetry()
{
    if (!AIoT.CheckConnect())
        return;

    float chipTemp = AIoT_Device.readChipTemp();
    uint32_t freeRam = AIoT_Device.readFreeRam();
    float baselineMean = hybridAI.edge.getDetector().getMean();
    float baselineStd = hybridAI.edge.getDetector().getStdDev();

    AIoT.updateTelemetry("chip_temp", chipTemp);
    AIoT.updateTelemetry("free_ram", (int)freeRam);
    AIoT.updateTelemetry("base_mean", baselineMean);
    AIoT.updateTelemetry("base_std", baselineStd);
    AIoT.updateTelemetry("motor_state", (int)AIoT_Device.Relay(PIN_MOTOR_RELAY));
    AIoT.updateTelemetry("fan_state", (int)AIoT_Device.Relay(PIN_COOLING_FAN));

    AIoT.sendTelemetry();
    Serial.println("[TELEMETRY] Da dong bo chi so he thong len Cloud Broker.");
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=======================================================");
    Serial.println("   AIoT_LIB: Industrial Hybrid AI Multi-Tier System");
    Serial.println("=======================================================");

    // 1. Đăng ký chân thiết bị động
    AIoT_Device.Relay(PIN_MOTOR_RELAY, "Dong co bang tai chinh");
    AIoT_Device.Relay(PIN_COOLING_FAN, "Quat hut nhiet tu dien");
    AIoT_Device.setBuzzerPin(PIN_BUZZER);

    // Mặc định bật động cơ hoạt động
    AIoT_Device.Relay(PIN_MOTOR_RELAY).on();

    // 2. Khởi tạo Hybrid AI
    hybridAI.setGeminiApiKey(GEMINI_KEY);
    hybridAI.begin(3.0f, 30);            // Ngưỡng 3.0 sigma, 30 mẫu hiệu chuẩn
    hybridAI.teachBaseline(5.0f, 0.5f);  // Dạy mẫu chuẩn ban đầu: 5.0 mm/s, std 0.5
    hybridAI.setAutoEmergencyActuation(false); // Xử lý logic can thiệp thủ công bên dưới

    // 3. Khởi tạo AIoT kết nối Cloud
    AIoT.begin(WIFI_SSID, WIFI_PASS, MQTT_USER, MQTT_PASS);

    Serial.println("[SYSTEM] He thong Hybrid AI cong nghiep da san sang.");
}

void loop()
{
    AIoT.run();

    // 1. Đọc mẫu và suy luận tại Tầng biên (Edge AI)
    float sample = readMotorSensor();
    EdgeAI::InferenceResult res = hybridAI.process(sample);

    // 2. Phản xạ an toàn tại chỗ (Local Fail-Safe Reflex < 1ms)
    if (res.isEmergency)
    {
        Serial.printf("[EDGE_AI CRITICAL] Phat hien su co nghiem trong! Gia tri: %.2f | Score: %.2f\n",
                      sample, res.score);

        // Cắt nguồn động cơ và bật quạt làm mát tối đa
        AIoT_Device.Relay(PIN_MOTOR_RELAY).off();
        AIoT_Device.Relay(PIN_COOLING_FAN).on();
        AIoT_Device.beep(150);

        // Kích hoạt Tầng 3 (Cloud AI Gemini) để chẩn đoán nguyên nhân (giới hạn 30s 1 lần)
        if (AIoT.CheckConnect() && (millis() - lastCloudDiagnosisMs > 30000))
        {
            lastCloudDiagnosisMs = millis();
            Serial.println("[CLOUD_AI] Dang yeu cau Gemini phan tich su co...");

            String prompt = "Dong co bang tai xuat hien rung dong dot bien: " + String(sample) +
                            " mm/s (Duong co so an toan: 5.0 mm/s). Hay chan doan nguyen nhan hu hong va de xuat phuong an xu ly.";
            String advice = hybridAI.gemini.ask(prompt, AIoT_Device.getHardwarePrompt());

            Serial.println("\n--- [GEMINI DIAGNOSIS REPORT] ---");
            Serial.println(advice);
            Serial.println("---------------------------------");

            // Tự động thực thi bất kỳ chỉ thị nào AI đề xuất
            AIoT_Device.executeCommand(advice);
        }
    }

    // 3. Gửi Telemetry định kỳ mỗi 3 giây
    if (millis() - lastTelemetryMs >= 3000)
    {
        lastTelemetryMs = millis();
        sendHybridTelemetry();
    }

    delay(100);
}
