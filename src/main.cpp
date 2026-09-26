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
// 1. ĐỊNH NGHĨA TRỌNG SỐ CHO MẠNG NƠ-RON ĐA MỤC TIÊU (MULTI-TASK LEARNING)
// ==============================================================================
// - Đầu vào (inputDim = 16): 4 kênh cảm biến x 4 đặc trưng (Mean, RMS, P2P, StdDev)
// - Đầu ra 1 (numLabels = 3): Softmax phân loại 3 trạng thái hệ thống:
//     Label 0: NORMAL (Bình thường)
//     Label 1: WARNING (Cảnh báo bất thường)
//     Label 2: CRITICAL_FAULT (Sự cố nguy hiểm)
// - Đầu ra 2 (numCmds = 2): Sigmoid điều khiển 2 cơ cấu chấp hành độc lập:
//     Cmd 0: Điểm bật Quạt giải nhiệt (0.0 .. 1.0)
//     Cmd 1: Điểm bật Còi báo động (0.0 .. 1.0)
// -> Tổng số đầu ra = 3 + 2 = 5 hàng. Ma trận W có kích thước: 5 x 16 = 80 trọng số.

const char *LABEL_NAMES[3] = {"NORMAL", "WARNING", "CRITICAL_FAULT"};

const float W[80] = {
    // -------------------------------------------------------------------------
    // [NHÓM 1: 3 HÀNG CHO SOFTMAX LABELS]
    // -------------------------------------------------------------------------
    // Hàng 0 - Label 0 (NORMAL): Trọng số âm với các xung đột biến rung/dòng/nhiệt
    -0.5f, -0.6f, -0.4f, -0.3f, -0.4f, -0.5f, -0.3f, -0.2f, -0.3f, -0.4f, -0.2f, -0.1f, -0.3f, -0.3f, -0.2f, -0.1f,
    // Hàng 1 - Label 1 (WARNING): Nhạy với sự gia tăng nhiệt độ và rung lắc vừa phải
    0.4f, 0.5f, 0.3f, 0.2f, 0.3f, 0.4f, 0.2f, 0.1f, 0.6f, 0.7f, 0.3f, 0.2f, 0.4f, 0.5f, 0.3f, 0.2f,
    // Hàng 2 - Label 2 (CRITICAL_FAULT): Nhạy cực mạnh với rung chấn dữ dội, sốc dòng và tiếng ồn lớn
    0.9f, 1.0f, 0.8f, 0.7f, 0.9f, 0.9f, 0.7f, 0.6f, 0.6f, 0.7f, 0.5f, 0.4f, 0.8f, 0.9f, 0.7f, 0.6f,

    // -------------------------------------------------------------------------
    // [NHÓM 2: 2 HÀNG CHO SIGMOID COMMANDS]
    // -------------------------------------------------------------------------
    // Hàng 3 - Cmd 0 (QUẠT GIẢI NHIỆT): Phụ thuộc chủ yếu vào nhiệt độ (Kênh 2) và dòng điện (Kênh 1)
    0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.4f, 0.2f, 0.1f, 0.9f, 0.8f, 0.5f, 0.3f, 0.1f, 0.1f, 0.0f, 0.0f,
    // Hàng 4 - Cmd 1 (CÒI HÚ BÁO ĐỘNG): Phụ thuộc vào biên độ rung lắc P2P (Kênh 0) và tiếng ồn (Kênh 3)
    0.8f, 0.9f, 0.9f, 0.6f, 0.5f, 0.6f, 0.4f, 0.3f, 0.3f, 0.4f, 0.2f, 0.1f, 0.8f, 0.8f, 0.7f, 0.5f};

// Vector Bias (Định thiên): 3 bias cho Softmax Labels + 2 bias cho Sigmoid Commands
const float b[5] = {
    0.5f,  // Label 0 (NORMAL): Ưu tiên nhận diện bình thường khi các đặc trưng ở mức thấp
    -0.2f, // Label 1 (WARNING)
    -1.0f, // Label 2 (CRITICAL_FAULT): Cần kích hoạt mạnh mới báo lỗi nguy hiểm
    -0.6f, // Cmd 0 (QUẠT): Bias âm để giữ quạt tắt ở điều kiện nhiệt bình thường
    -1.5f  // Cmd 1 (CÒI): Bias âm sâu để chống còi hú nhầm khi có nhiễu nhẹ
};

// Tham số chuẩn hóa Z-Score (Mean và StdDev của 16 đặc trưng từ tập dữ liệu huấn luyện)
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

    AIoT.device.Relay(PIN_RELAY_POWER).on(); // Mặc định bật nguồn hệ thống

    // [B] Cấu hình Edge AI Engine
    // 16 mẫu trượt cho mỗi kênh, 4 kênh cảm biến
    AIoT.edgeAI.begin(16, 4);

    // Nạp mô hình: 16 Inputs, 3 Labels (Softmax), 2 Commands (Sigmoid)
    AIoT.edgeAI.setModel(W, b, 16, 3, 2);

    // Cài đặt chuẩn hóa Z-Score cho 16 đặc trưng
    AIoT.edgeAI.setZScore(meanVals, stdDevVals);

    Serial.println(F("[SYSTEM] AIoT Multi-Task Edge AI Ready (3 Labels + 2 Commands)"));
}

// ==============================================================================
// 3. LOOP: THU THẬP, SUY LUẬN & ĐIỀU KHIỂN KẾT HỢP
// ==============================================================================
void loop()
{
    AIoT.run();

    // 1. Đọc tín hiệu 4 cảm biến và đẩy vào pipeline (tự động lọc Kalman & lưu buffer)
    AIoT.edgeAI.push(0, (float)analogRead(PIN_SENSOR_VIB));
    AIoT.edgeAI.push(1, (float)analogRead(PIN_SENSOR_CUR));
    AIoT.edgeAI.push(2, (float)analogRead(PIN_SENSOR_TEMP));
    AIoT.edgeAI.push(3, (float)analogRead(PIN_SENSOR_NOISE));

    // 2. Khi cửa sổ dữ liệu đã thu thập đủ 16 mẫu ở cả 4 kênh
    if (AIoT.edgeAI.isReady())
    {
        // =====================================================================
        // CHẠY SUY LUẬN ĐỒNG THỜI CẢ SOFTMAX VÀ SIGMOID
        // =====================================================================
        size_t winnerLabel = AIoT.edgeAI.predict();

        // ---------------------------------------------------------------------
        // [A] KẾT QUẢ TỪ NHÓM 1: SOFTMAX CLASSIFICATION (NHÃN TRẠNG THÁI HỆ THỐNG)
        // ---------------------------------------------------------------------
        float labelConfidence = AIoT.edgeAI.getWinnerConfidence(); // Xác suất của nhãn chiến thắng (0.0 .. 1.0)

        // In chẩn đoán ra Serial
        Serial.printf("[LABEL/SOFTMAX] System State: %s | Confidence: %.1f%%\n",
                      LABEL_NAMES[winnerLabel], labelConfidence * 100.0f);

        // Điều khiển thiết bị theo Nhãn hệ thống:
        switch (winnerLabel)
        {
        case 0: // NORMAL: Hoạt động bình thường
            AIoT.device.Relay(PIN_RELAY_LAMP).off();
            break;

        case 1: // WARNING: Cảnh báo bất thường -> Bật đèn cảnh báo
            AIoT.device.Relay(PIN_RELAY_LAMP).on();
            break;

        case 2: // CRITICAL_FAULT: Sự cố nghiêm trọng -> Bật đèn nhấp nháy, CẮT NGUỒN MÁY
            AIoT.device.Relay(PIN_RELAY_LAMP).on();
            AIoT.device.Relay(PIN_RELAY_POWER).off(); // Ngắt nguồn khẩn cấp để bảo vệ động cơ
            Serial.println(F("[SAFETY] EMERGENCY SHUTDOWN TRIGGERED BY EDGE AI!"));
            break;
        }

        // ---------------------------------------------------------------------
        // [B] KẾT QUẢ TỪ NHÓM 2: SIGMOID COMMANDS (ĐIỀU KHIỂN THIẾT BỊ ĐỘC LẬP)
        // ---------------------------------------------------------------------
        // Lấy điểm số xác suất độc lập của từng cơ cấu chấp hành (0.0 .. 1.0)
        float fanScore = AIoT.edgeAI.getCmdScore(0);    // Điểm bật quạt tản nhiệt
        float buzzerScore = AIoT.edgeAI.getCmdScore(1); // Điểm kích hoạt còi hú

        Serial.printf("[CMD/SIGMOID] Fan Score: %.1f%% | Buzzer Score: %.1f%%\n",
                      fanScore * 100.0f, buzzerScore * 100.0f);

        // Điều khiển Quạt: Độc lập với nhãn trạng thái (kể cả NORMAL nhưng nhiệt hơi ấm vẫn bật làm mát)
        if (fanScore >= 0.60f) // Ngưỡng 60%
        {
            AIoT.device.Relay(PIN_RELAY_FAN).on();
        }
        else
        {
            AIoT.device.Relay(PIN_RELAY_FAN).off();
        }

        // Điều khiển Còi: Độc lập, chỉ hú khi độ tự tin nguy cấp vượt ngưỡng 75%
        if (buzzerScore >= 0.75f) // Ngưỡng 75% chống báo động giả
        {
            AIoT.device.Relay(PIN_RELAY_BUZZER).on();
        }
        else
        {
            AIoT.device.Relay(PIN_RELAY_BUZZER).off();
        }

        // ---------------------------------------------------------------------
        // [C] BẮN TẤT CẢ DỮ LIỆU LÊN CLOUD TELEMETRY DASHBOARD
        // ---------------------------------------------------------------------
        AIoT.updateTelemetry("system_label", LABEL_NAMES[winnerLabel]);
        AIoT.updateTelemetry("label_confidence", labelConfidence * 100.0f);
        AIoT.updateTelemetry("fan_score", fanScore * 100.0f);
        AIoT.updateTelemetry("buzzer_score", buzzerScore * 100.0f);
        AIoT.updateTelemetry("ai_latency_us", (int)AIoT.edgeAI.getExecutionTime());
    }

    delay(20); // Chu kỳ lấy mẫu 50Hz (20ms)
}