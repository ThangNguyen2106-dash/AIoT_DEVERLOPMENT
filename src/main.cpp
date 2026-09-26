#define DEBUG_COLOR
#include <Arduino.h>
#include <AIoT.h>

// 4 Cảm biến đầu vào
#define PIN_SENSOR_VIB 32   // Rung động
#define PIN_SENSOR_CUR 33   // Dòng điện
#define PIN_SENSOR_TEMP 34  // Nhiệt độ
#define PIN_SENSOR_NOISE 35 // Tiếng ồn

// 4 Cơ cấu chấp hành điều khiển độc lập qua Sigmoid
#define PIN_RELAY_FAN 12    // Lệnh 0: Quạt giải nhiệt
#define PIN_RELAY_LAMP 13   // Lệnh 1: Đèn cảnh báo
#define PIN_RELAY_BUZZER 14 // Lệnh 2: Còi hú
#define PIN_RELAY_POWER 15  // Lệnh 3: Nguồn điện chính (Ngắt khi nguy hiểm)

// ==============================================================================
// 1. TRỌNG SỐ CHO 4 ĐẦU RA SIGMOID (4 Lệnh x 16 Inputs = 64 Trọng số)
// ==============================================================================
// 16 đầu vào: 4 kênh cảm biến x 4 đặc trưng (Mean, RMS, P2P, StdDev)
const char *LABEL_NAMES[3] = {"NORMAL", "WARNING", "CRITICAL_FAULT"};
const float W[80] = {
    // --- [NHÓM 1: 3 HÀNG CHO SOFTMAX LABELS (0, 1, 2)] ---
    // Hàng 0 - Label 0 (NORMAL): Trọng số âm khi rung, dòng, nhiệt tăng cao
    -0.5f, -0.6f, -0.4f, -0.3f, -0.4f, -0.5f, -0.3f, -0.2f, -0.3f, -0.4f, -0.2f, -0.1f, -0.3f, -0.3f, -0.2f, -0.1f,
    // Hàng 1 - Label 1 (WARNING): Nhạy với nhiệt độ và rung chấn vừa phải
    0.4f, 0.5f, 0.3f, 0.2f, 0.3f, 0.4f, 0.2f, 0.1f, 0.6f, 0.7f, 0.3f, 0.2f, 0.4f, 0.5f, 0.3f, 0.2f,
    // Hàng 2 - Label 2 (CRITICAL_FAULT): Phản ứng rất mạnh với sốc rung, quá dòng, tiếng ồn lớn
    0.9f, 1.0f, 0.8f, 0.7f, 0.9f, 0.9f, 0.7f, 0.6f, 0.6f, 0.7f, 0.5f, 0.4f, 0.8f, 0.9f, 0.7f, 0.6f,
    // --- [NHÓM 2: 2 HÀNG CHO SIGMOID COMMANDS (0, 1)] ---
    // Hàng 3 - Cmd 0 (QUẠT GIẢI NHIỆT): Phụ thuộc vào Nhiệt độ (Kênh 2) và Dòng điện (Kênh 1)
    0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.4f, 0.2f, 0.1f, 0.9f, 0.8f, 0.5f, 0.3f, 0.1f, 0.1f, 0.0f, 0.0f,
    // Hàng 4 - Cmd 1 (CÒI BÁO ĐỘNG): Phụ thuộc vào độ lệch chuẩn biên độ rung (Kênh 0) và tiếng ồn (Kênh 3)
    0.8f, 0.9f, 0.9f, 0.6f, 0.5f, 0.6f, 0.4f, 0.3f, 0.3f, 0.4f, 0.2f, 0.1f, 0.8f, 0.8f, 0.7f, 0.5f};
// 3 Bias đầu cho Labels + 2 Bias sau cho Commands:
const float b[5] = {
    0.5f,  // Bias Label 0 (NORMAL)
    -0.2f, // Bias Label 1 (WARNING)
    -1.0f, // Bias Label 2 (FAULT)
    -0.6f, // Bias Cmd 0 (QUẠT)
    -1.5f  // Bias Cmd 1 (CÒI)
};

// Tham số Z-Score từ tập dữ liệu học (Kỳ vọng Mean và Độ lệch chuẩn StdDev của 16 đặc trưng)
const float meanVals[16] = {
    25.0f, 26.0f, 5.0f, 1.2f, // Kênh 0 (Rung)
    10.0f, 10.5f, 2.0f, 0.5f, // Kênh 1 (Dòng điện)
    45.0f, 45.2f, 3.0f, 0.8f, // Kênh 2 (Nhiệt độ)
    50.0f, 51.0f, 6.0f, 1.5f  // Kênh 3 (Tiếng ồn)
};

const float stdDevVals[16] = {
    4.5f, 4.8f, 1.5f, 0.4f, // Kênh 0
    2.0f, 2.1f, 0.8f, 0.2f, // Kênh 1
    5.0f, 5.1f, 1.2f, 0.3f, // Kênh 2
    8.0f, 8.2f, 2.0f, 0.5f  // Kênh 3
};

// ==============================================================================
// 2. SETUP
// ==============================================================================
void setup()
{
    Serial.begin(115200);
    AIoT.device.begin();
    // Khởi tạo Edge AI: Cửa sổ 16 mẫu, 4 kênh cảm biến
    AIoT.edgeAI.begin(16, 4);
    // [C] Cơ chế 2 tầng: Ưu tiên nạp mô hình thích nghi từ NVS Flash
    if (AIoT.edgeAI.loadFromNVS())
    {
        Serial.printf("[EDGE_AI] Successfully loaded adaptive model from NVS Flash! (Norm: %s)\n",
                      AIoT.edgeAI.getNormTypeName());
    }
    else
    {
        Serial.println(F("[EDGE_AI] NVS Flash is empty. Using factory default model (Z-Score):"));
        AIoT.edgeAI.setModel(W, b, 16, 3, 2);
        AIoT.edgeAI.setZScore(meanVals, stdDevVals);
    }
}
void loop()
{
    AIoT.run();
    // 1. Đọc và đẩy 4 cảm biến vào pipeline
    AIoT.edgeAI.push(0, (float)analogRead(PIN_SENSOR_VIB));
    AIoT.edgeAI.push(1, (float)analogRead(PIN_SENSOR_CUR));
    AIoT.edgeAI.push(2, (float)analogRead(PIN_SENSOR_TEMP));
    AIoT.edgeAI.push(3, (float)analogRead(PIN_SENSOR_NOISE));
    if (AIoT.edgeAI.isReady())
    {
        // 2. Chạy suy luận toàn bộ mô hình
        size_t winnerLabel = AIoT.edgeAI.predict();
        // -----------------------------------------------------------------
        // [A] TRUY XUẤT NHÓM SOFTMAX LABELS
        // -----------------------------------------------------------------
        float labelConfidence = AIoT.edgeAI.getWinnerConfidence(); // 0.0 .. 1.0
        Serial.printf("[LABEL] %s (%.1f%%)\n", LABEL_NAMES[winnerLabel], labelConfidence * 100.0f);
        switch (winnerLabel)
        {
        case 0: // NORMAL
            AIoT.device.Relay(PIN_RELAY_LAMP).off();
            break;
        case 1: // WARNING
            AIoT.device.Relay(PIN_RELAY_LAMP).on();
            break;
        case 2: // CRITICAL_FAULT
            AIoT.device.Relay(PIN_RELAY_LAMP).on();
            AIoT.device.Relay(PIN_RELAY_POWER).off(); // Cắt nguồn máy khẩn cấp
            break;
        }
        // -----------------------------------------------------------------
        // [B] TRUY XUẤT NHÓM SIGMOID COMMANDS (ĐỘC LẬP)
        // -----------------------------------------------------------------
        float fanScore = AIoT.edgeAI.getCmdScore(0);    // Điểm bật quạt (0.0 .. 1.0)
        float buzzerScore = AIoT.edgeAI.getCmdScore(1); // Điểm bật còi (0.0 .. 1.0)
        // Quạt bật khi điểm >= 60% (kể cả trạng thái máy đang là NORMAL nhưng nhiệt hơi ấm)
        if (fanScore >= 0.60f)
            AIoT.device.Relay(PIN_RELAY_FAN).on();
        else
            AIoT.device.Relay(PIN_RELAY_FAN).off();
        // Còi hú khi điểm nguy cấp >= 75%
        if (buzzerScore >= 0.75f)
            AIoT.device.Relay(PIN_RELAY_BUZZER).on();
        else
            AIoT.device.Relay(PIN_RELAY_BUZZER).off();
        // -----------------------------------------------------------------
        // [C] TELEMETRY DASHBOARD
        // -----------------------------------------------------------------
        AIoT.updateTelemetry("system_label", LABEL_NAMES[winnerLabel]);
        AIoT.updateTelemetry("label_confidence", labelConfidence * 100.0f);
        AIoT.updateTelemetry("fan_score", fanScore * 100.0f);
        AIoT.updateTelemetry("buzzer_score", buzzerScore * 100.0f);
        AIoT.updateTelemetry("ai_latency_us", (int)AIoT.edgeAI.getExecutionTime());
    }
    delay(20);
}