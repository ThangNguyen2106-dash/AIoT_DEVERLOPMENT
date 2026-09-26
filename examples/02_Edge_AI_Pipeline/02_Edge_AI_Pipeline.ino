/**
 * ==============================================================================
 * VÍ DỤ 02: EDGE AI MULTI-TASK PIPELINE (ON-DEVICE REAL-TIME INFERENCE)
 * ==============================================================================
 * Hướng dẫn vận hành toàn bộ Pipeline suy luận tại biên (Edge AI):
 * 1. Thu thập dữ liệu từ 4 kênh cảm biến (Rung động, Dòng điện, Nhiệt độ, Tiếng ồn)
 * 2. Tự động lọc nhiễu thích nghi Kalman & lưu đệm trượt 16 mẫu
 * 3. Tự động trích xuất 16 đặc trưng vật lý (Mean, RMS, P2P, StdDev)
 * 4. Chuẩn hóa phân phối chuẩn Z-Score
 * 5. Mạng nơ-ron đa mục tiêu chạy đồng thời:
 *    - Softmax: Phân loại trạng thái hệ thống (NORMAL, WARNING, FAULT)
 *    - Sigmoid: Điều khiển cơ cấu chấp hành độc lập (Quạt giải nhiệt, Còi báo động)
 * ==============================================================================
 */

#define DEBUG_COLOR
#include <Arduino.h>
#include <AIoT.h>

// 4 Cảm biến đầu vào (4 Kênh ADC)
#define PIN_SENSOR_VIB 32   // Kênh 0: Rung động
#define PIN_SENSOR_CUR 33   // Kênh 1: Dòng điện
#define PIN_SENSOR_TEMP 34  // Kênh 2: Nhiệt độ
#define PIN_SENSOR_NOISE 35 // Kênh 3: Tiếng ồn

// Cơ cấu chấp hành
#define PIN_RELAY_FAN 12    // Quạt tản nhiệt (theo Sigmoid Cmd 0)
#define PIN_RELAY_BUZZER 14 // Còi hú (theo Sigmoid Cmd 1)
#define PIN_RELAY_LAMP 13   // Đèn trạng thái (theo Softmax Label)
#define PIN_RELAY_POWER 15  // Nguồn chính (Ngắt khi Critical Fault)

const char *LABEL_NAMES[3] = {"NORMAL", "WARNING", "CRITICAL_FAULT"};

// Ma trận trọng số W (5 hàng x 16 đầu vào = 80 trọng số)
// - Hàng 0, 1, 2: 3 Softmax Labels
// - Hàng 3, 4: 2 Sigmoid Commands
const float W[80] = {
    // Hàng 0 - Label 0 (NORMAL)
    -0.5f, -0.6f, -0.4f, -0.3f, -0.4f, -0.5f, -0.3f, -0.2f, -0.3f, -0.4f, -0.2f, -0.1f, -0.3f, -0.3f, -0.2f, -0.1f,
    // Hàng 1 - Label 1 (WARNING)
    0.4f, 0.5f, 0.3f, 0.2f, 0.3f, 0.4f, 0.2f, 0.1f, 0.6f, 0.7f, 0.3f, 0.2f, 0.4f, 0.5f, 0.3f, 0.2f,
    // Hàng 2 - Label 2 (CRITICAL_FAULT)
    0.9f, 1.0f, 0.8f, 0.7f, 0.9f, 0.9f, 0.7f, 0.6f, 0.6f, 0.7f, 0.5f, 0.4f, 0.8f, 0.9f, 0.7f, 0.6f,
    // Hàng 3 - Cmd 0 (QUẠT GIẢI NHIỆT)
    0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.4f, 0.2f, 0.1f, 0.9f, 0.8f, 0.5f, 0.3f, 0.1f, 0.1f, 0.0f, 0.0f,
    // Hàng 4 - Cmd 1 (CÒI BÁO ĐỘNG)
    0.8f, 0.9f, 0.9f, 0.6f, 0.5f, 0.6f, 0.4f, 0.3f, 0.3f, 0.4f, 0.2f, 0.1f, 0.8f, 0.8f, 0.7f, 0.5f};

const float b[5] = {0.5f, -0.2f, -1.0f, -0.6f, -1.5f};

// Tham số Z-Score huấn luyện
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

void setup()
{
    Serial.begin(115200);

    // 1. Khởi tạo thiết bị phần cứng qua HAL
    AIoT.device.begin();
    AIoT.device.attachRelay(PIN_RELAY_FAN, "Cooling_Fan");
    AIoT.device.attachRelay(PIN_RELAY_BUZZER, "Alarm_Buzzer");
    AIoT.device.attachRelay(PIN_RELAY_LAMP, "Status_Lamp");
    AIoT.device.attachRelay(PIN_RELAY_POWER, "Main_Power");

    AIoT.device.Relay(PIN_RELAY_POWER).on(); // Bật sẵn nguồn động cơ

    // 2. Khởi tạo Edge AI Pipeline: Cửa sổ 16 mẫu, 4 kênh cảm biến
    AIoT.edgeAI.begin(16, 4);

    // 3. Cấu hình mô hình: 16 Inputs, 3 Labels, 2 Commands
    AIoT.edgeAI.setModel(W, b, 16, 3, 2);

    // 4. Kích hoạt chuẩn hóa Z-Score
    AIoT.edgeAI.setZScore(meanVals, stdDevVals);

    Serial.println(F("[SYSTEM] Edge AI Pipeline Ready!"));
}

void loop()
{
    AIoT.run();

    // 1. Đọc và đẩy 4 cảm biến vào pipeline (tự động lọc Kalman)
    AIoT.edgeAI.push(0, (float)analogRead(PIN_SENSOR_VIB));
    AIoT.edgeAI.push(1, (float)analogRead(PIN_SENSOR_CUR));
    AIoT.edgeAI.push(2, (float)analogRead(PIN_SENSOR_TEMP));
    AIoT.edgeAI.push(3, (float)analogRead(PIN_SENSOR_NOISE));

    // 2. Khi bộ đệm đủ 16 mẫu trượt
    if (AIoT.edgeAI.isReady())
    {
        // Chạy suy luận mạng nơ-ron
        size_t winner = AIoT.edgeAI.predict();
        float conf = AIoT.edgeAI.getWinnerConfidence();

        // Lấy điểm số độc lập các lệnh Sigmoid
        float fanScore = AIoT.edgeAI.getCmdScore(0);
        float buzzerScore = AIoT.edgeAI.getCmdScore(1);

        Serial.printf("[EDGE_AI] State: %s (%.1f%%) | Fan: %.1f%% | Buzzer: %.1f%% | Exec: %lu us\n",
                      LABEL_NAMES[winner], conf * 100.0f,
                      fanScore * 100.0f, buzzerScore * 100.0f,
                      (unsigned long)AIoT.edgeAI.getExecutionTime());

        // Điều khiển bảo vệ theo trạng thái hệ thống:
        if (winner == 2) // CRITICAL_FAULT
        {
            AIoT.device.Relay(PIN_RELAY_LAMP).on();
            AIoT.device.Relay(PIN_RELAY_POWER).off(); // Ngắt nguồn máy khẩn cấp
        }
        else if (winner == 1) // WARNING
        {
            AIoT.device.Relay(PIN_RELAY_LAMP).on();
        }
        else
        {
            AIoT.device.Relay(PIN_RELAY_LAMP).off();
        }

        // Điều khiển quạt độc lập theo điểm xác suất Sigmoid
        if (fanScore >= 0.60f)
            AIoT.device.Relay(PIN_RELAY_FAN).on();
        else
            AIoT.device.Relay(PIN_RELAY_FAN).off();

        // Điều khiển còi độc lập
        if (buzzerScore >= 0.75f)
            AIoT.device.Relay(PIN_RELAY_BUZZER).on();
        else
            AIoT.device.Relay(PIN_RELAY_BUZZER).off();
    }

    delay(20); // Chu kỳ lấy mẫu 50Hz (20ms)
}
