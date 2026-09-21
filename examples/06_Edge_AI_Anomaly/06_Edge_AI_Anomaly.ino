/**
 * ============================================================================
 * AIoT_LIB - Ví Dụ 06: Phát Hiện Dị Thường Tại Biên (Edge AI Anomaly Detection)
 * ============================================================================
 * Chức năng:
 * - Tự động học đường cơ sở (Baseline Auto-Calibration) trong 20 mẫu ban đầu
 * - Đánh giá độ lệch Z-Score theo thời gian thực với giải thuật Welford O(1) RAM
 * - Phản xạ bảo vệ máy khẩn cấp tại chỗ (< 1ms): Tự ngắt Relay khi rung chấn
 *   vượt ngưỡng nguy hiểm (Critical), hoàn toàn độc lập không cần kết nối mạng.
 * ============================================================================
 */

#include <Arduino.h>
#include <AIoT.h>
#include <EdgeAI/EdgeAI.h>

#define PIN_RELAY_MOTOR 14 // Rơ-le cấp nguồn cho động cơ
#define PIN_BUZZER      4  // Còi báo động sự cố

EdgeAI::Engine edgeAI;

// Giả lập đọc cảm biến rung chấn động cơ (mm/s)
float readVibrationSensor()
{
    static unsigned long cycle = 0;
    cycle++;

    // Tín hiệu bình thường quanh mức 5.0 mm/s
    float noise = (float)(random(-5, 5)) / 10.0f;
    float value = 5.0f + noise;

    // Giả lập sự cố rung lắc mạnh (kẹt bạc đạn) mỗi 20 chu kỳ
    if (cycle % 20 == 0)
    {
        value = 22.0f; // Rung chấn cực lớn vượt ngưỡng
    }
    return value;
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== AIoT_LIB: Vi Du 06 - Edge AI Anomaly Detection ===");

    // 1. Đăng ký phần cứng
    AIoT_Device.Relay(PIN_RELAY_MOTOR, "Dong co bang tai");
    AIoT_Device.setBuzzerPin(PIN_BUZZER);
    AIoT_Device.Relay(PIN_RELAY_MOTOR).on(); // Khởi động động cơ

    // 2. Khởi tạo Edge AI: Ngưỡng 3.0 sigma, tự học 20 mẫu ban đầu
    edgeAI.begin(3.0f, 20);

    Serial.println("He thong dang thu thap 20 mau de tu hoc duong co so (Baseline)...");
}

void loop()
{
    float sample = readVibrationSensor();

    // Thực hiện suy luận TinyML tại chỗ (< 1ms)
    EdgeAI::InferenceResult result = edgeAI.process(sample);

    if (edgeAI.getDetector().isCalibrated())
    {
        Serial.printf("[EDGE_AI] Gia tri: %.2f mm/s | Z-Score: %.2f | Trang thai: %s\n",
                      sample, result.zScore, result.label);

        // PHẢN XẠ BẢO VỆ TẠI CHỖ (LOCAL FAIL-SAFE REFLEX)
        if (result.isEmergency)
        {
            Serial.println(">>> NGUY HIEM! Tu dong cat nguon Dong co & Phat coi!");
            AIoT_Device.Relay(PIN_RELAY_MOTOR).off(); // Ngắt điện bảo vệ tức thì
            AIoT_Device.beep(200);
        }
        else if (result.isAnomaly)
        {
            Serial.println("[CANH BAO] Tin hieu bat thuong nhe.");
        }
    }
    else
    {
        Serial.printf("Dang hieu chuan baseline... [%u/20 mau]\n",
                      edgeAI.getDetector().getSampleCount());
    }

    delay(300);
}
