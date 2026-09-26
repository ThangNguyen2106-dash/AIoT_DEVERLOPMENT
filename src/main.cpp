#define DEBUG_COLOR
#include <Arduino.h>
#include <AIoT.h>

// 4 Cảm biến đầu vào (4 Kênh ADC)
#define PIN_SENSOR_VIB 32   // Kênh 0: Rung động
#define PIN_SENSOR_CUR 33   // Kênh 1: Dòng điện
#define PIN_SENSOR_TEMP 34  // Kênh 2: Nhiệt độ
#define PIN_SENSOR_NOISE 35 // Kênh 3: Tiếng ồn

// 4 Rơ-le cơ cấu chấp hành
#define PIN_RELAY_FAN 12    // Điều khiển theo Sigmoid Cmd 0: Quạt giải nhiệt
#define PIN_RELAY_BUZZER 14 // Điều khiển theo Sigmoid Cmd 1: Còi hú cảnh báo
#define PIN_RELAY_LAMP 13   // Điều khiển theo Softmax Label: Đèn báo trạng thái (Warning/Fault)
#define PIN_RELAY_POWER 15  // Điều khiển theo Softmax Label: Nguồn máy chính (Ngắt khi Critical Fault)

// ==============================================================================
// 1. TRỌNG SỐ MẶC ĐỊNH XUẤT XƯỞNG (3 Labels + 2 Commands x 16 Inputs = 80 Weights)
// ==============================================================================
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
    // Hàng 4 - Cmd 1 (CÒI BÁO ĐỘNG): Phụ thuộc vào biên độ rung lắc P2P (Kênh 0) và tiếng ồn (Kênh 3)
    0.8f, 0.9f, 0.9f, 0.6f, 0.5f, 0.6f, 0.4f, 0.3f, 0.3f, 0.4f, 0.2f, 0.1f, 0.8f, 0.8f, 0.7f, 0.5f};

// Vector Bias mặc định: 3 bias cho Softmax Labels + 2 bias cho Sigmoid Commands
const float b[5] = {
    0.5f,  // Bias Label 0 (NORMAL)
    -0.2f, // Bias Label 1 (WARNING)
    -1.0f, // Bias Label 2 (FAULT)
    -0.6f, // Bias Cmd 0 (QUẠT)
    -1.5f  // Bias Cmd 1 (CÒI)
};

// Tham số Z-Score mặc định từ tập dữ liệu huấn luyện
const float meanVals[16] = {
    25.0f, 26.0f, 5.0f, 1.2f, // Kênh 0 (Rung động)
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

    // [A] Khởi tạo thiết bị phần cứng qua HAL
    AIoT.device.begin();
    AIoT.device.attachRelay(PIN_RELAY_FAN, "Cooling_Fan");
    AIoT.device.attachRelay(PIN_RELAY_BUZZER, "Alarm_Buzzer");
    AIoT.device.attachRelay(PIN_RELAY_LAMP, "Status_Lamp");
    AIoT.device.attachRelay(PIN_RELAY_POWER, "Main_Power");

    AIoT.device.Relay(PIN_RELAY_POWER).on(); // Bật sẵn nguồn động cơ

    // [B] Khởi tạo Edge AI: Cửa sổ 16 mẫu trượt, 4 kênh cảm biến
    AIoT.edgeAI.begin(16, 4);

    // [C] Cơ chế nạp 2 tầng (Fallback Architecture):
    // TẦNG 1: Thử nạp mô hình thích nghi mới nhất đã lưu trong NVS Flash
    if (AIoT.edgeAI.loadFromNVS())
    {
        Serial.printf("[EDGE_AI] Successfully loaded adaptive model from NVS Flash! (Norm: %s)\n",
                      AIoT.edgeAI.getNormTypeName());
    }
    // TẦNG 2: Nếu Flash NVS chưa có dữ liệu -> Dùng mô hình mặc định xuất xưởng
    else
    {
        Serial.println(F("[EDGE_AI] NVS Flash empty -> Using factory default model (Z-Score):"));
        AIoT.edgeAI.setModel(W, b, 16, 3, 2);
        AIoT.edgeAI.setZScore(meanVals, stdDevVals);
    }
}

// ==============================================================================
// 3. LOOP: ĐỌC DỮ LIỆU, SUY LUẬN & ĐIỀU KHIỂN ĐỒNG THỜI
// ==============================================================================
void loop()
{
    AIoT.run();

    // 1. Đọc và đẩy tín hiệu 4 cảm biến vào pipeline (tự động lọc Kalman)
    AIoT.edgeAI.push(0, (float)analogRead(PIN_SENSOR_VIB));
    AIoT.edgeAI.push(1, (float)analogRead(PIN_SENSOR_CUR));
    AIoT.edgeAI.push(2, (float)analogRead(PIN_SENSOR_TEMP));
    AIoT.edgeAI.push(3, (float)analogRead(PIN_SENSOR_NOISE));

    // 2. Khi bộ đệm đã tích lũy đủ 16 mẫu ở cả 4 kênh
    if (AIoT.edgeAI.isReady())
    {
        // Thực thi suy luận đa mục tiêu (Softmax + Sigmoid)
        size_t winnerLabel = AIoT.edgeAI.predict();

        // -----------------------------------------------------------------
        // [A] TRUY XUẤT NHÓM 1: SOFTMAX CLASSIFICATION (NHÃN TRẠNG THÁI)
        // -----------------------------------------------------------------
        float labelConfidence = AIoT.edgeAI.getWinnerConfidence(); // 0.0 .. 1.0
        Serial.printf("[LABEL] System State: %s (%.1f%%)\n", LABEL_NAMES[winnerLabel], labelConfidence * 100.0f);

        // Điều khiển bảo vệ theo trạng thái hệ thống:
        switch (winnerLabel)
        {
        case 0: // NORMAL: Bình thường
            AIoT.device.Relay(PIN_RELAY_LAMP).off();
            break;

        case 1: // WARNING: Cảnh báo bất thường -> Bật đèn cảnh báo
            AIoT.device.Relay(PIN_RELAY_LAMP).on();
            break;

        case 2: // CRITICAL_FAULT: Sự cố nguy hiểm -> Bật đèn, CẮT NGUỒN ĐỘNG CƠ
            AIoT.device.Relay(PIN_RELAY_LAMP).on();
            AIoT.device.Relay(PIN_RELAY_POWER).off(); // Ngắt điện khẩn cấp
            Serial.println(F("[SAFETY] EMERGENCY TRIP ACTIVATED!"));
            break;
        }

        // -----------------------------------------------------------------
        // [B] TRUY XUẤT NHÓM 2: SIGMOID COMMANDS (ĐIỀU KHIỂN THIẾT BỊ ĐỘC LẬP)
        // -----------------------------------------------------------------
        float fanScore = AIoT.edgeAI.getCmdScore(0);    // Điểm bật quạt (0.0 .. 1.0)
        float buzzerScore = AIoT.edgeAI.getCmdScore(1); // Điểm bật còi hú (0.0 .. 1.0)

        Serial.printf("[CMD] Fan Score: %.1f%% | Buzzer Score: %.1f%%\n",
                      fanScore * 100.0f, buzzerScore * 100.0f);

        // Quạt bật khi điểm >= 60% (độc lập, nhiệt hơi ấm quạt tự bật làm mát)
        if (fanScore >= 0.60f)
            AIoT.device.Relay(PIN_RELAY_FAN).on();
        else
            AIoT.device.Relay(PIN_RELAY_FAN).off();

        // Còi hú khi điểm nguy cấp >= 75% (chống báo động giả)
        if (buzzerScore >= 0.75f)
            AIoT.device.Relay(PIN_RELAY_BUZZER).on();
        else
            AIoT.device.Relay(PIN_RELAY_BUZZER).off();

        // -----------------------------------------------------------------
        // [C] BẮN DỮ LIỆU LÊN CLOUD TELEMETRY DASHBOARD
        // -----------------------------------------------------------------
        AIoT.updateTelemetry("system_label", LABEL_NAMES[winnerLabel]);
        AIoT.updateTelemetry("label_confidence", labelConfidence * 100.0f);
        AIoT.updateTelemetry("fan_score", fanScore * 100.0f);
        AIoT.updateTelemetry("buzzer_score", buzzerScore * 100.0f);
        AIoT.updateTelemetry("ai_latency_us", (int)AIoT.edgeAI.getExecutionTime());
    }

    delay(20); // Chu kỳ lấy mẫu 50Hz (20ms)
}