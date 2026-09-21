/**
 * ============================================================================
 * AIoT_LIB - Ví Dụ 03: Phát Hiện Bất Thường Bằng Edge AI (TinyML Anomaly Detection)
 * ============================================================================
 * Mô tả:
 * - Thuật toán học phân phối chuẩn trực tuyến Welford O(1) RAM & CPU
 * - Tự động cân chỉnh đường cơ sở (Baseline Auto-calibration) từ cảm biến
 * - Phát hiện rung động bất thường, quá dòng, quá nhiệt tức thời trong < 1ms
 * - Phản xạ bảo vệ thiết bị tại chỗ (ngắt Relay, hú còi) mà không cần Internet
 */

#include <Arduino.h>
#include <AIoT.h>
#include <EdgeAI/EdgeAI.h>

// Định nghĩa chân phần cứng tùy ý theo thiết kế của bạn
#define PIN_RELAY_MOTOR 14 // Rơ-le cấp nguồn động cơ băng tải
#define PIN_BUZZER      4  // Còi cảnh báo sự cố

EdgeAI::Engine edgeAI;

// Giả lập tín hiệu cảm biến rung động (mm/s) hoặc nhiệt độ
float readVibrationSensor()
{
    static unsigned long cycle = 0;
    cycle++;

    // Tín hiệu bình thường dao động quanh 10.0 kèm nhiễu trắng nhỏ
    float noise = (float)(random(-10, 10)) / 10.0f;
    float value = 10.0f + noise;

    // Giả lập tạo lỗi đột biến rung giật cơ khí mỗi 25 chu kỳ
    if (cycle % 25 == 0)
    {
        value = 32.5f; // Rung lắc cực mạnh vượt ngưỡng an toàn
    }
    return value;
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    // 1. Đăng ký chân thiết bị động
    AIoT_Device.Relay(PIN_RELAY_MOTOR, "Dong co bang tai");
    AIoT_Device.setBuzzerPin(PIN_BUZZER);

    // Bật động cơ ở trạng thái khởi động ban đầu
    AIoT_Device.Relay(PIN_RELAY_MOTOR).on();

    // 2. Khởi tạo Edge AI: Ngưỡng 3.0 sigma, 30 mẫu ban đầu để tự cân chỉnh baseline
    edgeAI.begin(3.0f, 30);

    Serial.println("\n=======================================================");
    Serial.println("   AIoT_LIB: On-Device Edge AI Anomaly Detection");
    Serial.println("=======================================================");
    Serial.println("Giai doan 1: Dang hoc duong co so (Calibrating 30 samples)...");
}

void loop()
{
    float sensorSample = readVibrationSensor();

    // Chạy suy luận máy học biên tại chỗ (< 1ms)
    EdgeAI::InferenceResult result = edgeAI.process(sensorSample);

    if (edgeAI.getDetector().isCalibrated())
    {
        Serial.printf("[EDGE_AI] Gia tri: %.2f | Z-Score: %.2f | Ket luan: %s\n",
                      sensorSample, result.score, result.label);

        // PHẢN XẠ BẢO VỆ TẠI CHỖ (REFLEX FAILSAFE)
        if (result.isEmergency)
        {
            Serial.println("[FAILSAFE_ACTION] NGUY HIEM! Tu dong cat Relay Dong co & Phat coi!");
            AIoT_Device.Relay(PIN_RELAY_MOTOR).off(); // Cắt điện ngay lập tức
            AIoT_Device.beep(250);                    // Cảnh báo âm thanh
        }
        else if (result.isAnomaly)
        {
            Serial.println("[WARNING] Phat hien dao dong bat thuong nhung chua nguy cap.");
        }
        else
        {
            // Trạng thái bình thường: Duy trì hoạt động an toàn
            if (!AIoT_Device.Relay(PIN_RELAY_MOTOR))
            {
                AIoT_Device.Relay(PIN_RELAY_MOTOR).on();
            }
        }
    }
    else
    {
        Serial.printf("Dang hieu chuan baseline... [%u/30 mau]\n",
                      edgeAI.getDetector().getSampleCount());
    }

    delay(200);
}
