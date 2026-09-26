#define DEBUG_COLOR // Bật log màu ANSI trên Serial Monitor (bỏ dòng này khi nạp bản sản xuất)
#include <Arduino.h>
#include <AIoT.h>

// ==============================================================================
// 1. TRỌNG SỐ MÔ HÌNH NƠ-RON & THAM SỐ CHUẨN HÓA (Xuất từ Google Colab / Python)
// ==============================================================================
// Giả sử mô hình có:
// - 4 đặc trưng đầu vào: [Mean, RMS, P2P, StdDev]
// - 2 nhãn phân loại: [0: Bình thường (Normal), 1: Sự cố / Bất thường (Anomaly)]
const float W[8] = {
    0.12f, 0.45f, 0.67f, 0.23f, // Trọng số cho Nhãn 0
    0.85f, 0.91f, 0.74f, 0.62f  // Trọng số cho Nhãn 1
};
const float b[2] = {0.05f, -0.10f}; // Bias định thiên

// Dải giá trị Min - Max của 4 đặc trưng dùng để chuẩn hóa về [0.0, 1.0]
const float minVals[4] = {10.0f, 5.0f, 0.0f, 0.0f};
const float maxVals[4] = {80.0f, 60.0f, 40.0f, 15.0f};

// ==============================================================================
// 2. SETUP: KHỞI TẠO HỆ THỐNG
// ==============================================================================
void setup()
{
    Serial.begin(115200);

    // [A] Khởi tạo thiết bị ngoại vi (Device HAL)
    AIoT.device.begin();
    AIoT.device.attachRelay(14, "Warning_Relay"); // Rơ-le cảnh báo trên GPIO 14

    // [B] Khởi tạo Edge AI Pipeline (Chỉ cần 3 dòng lệnh)
    AIoT.edgeAI.begin(16);                          // Cửa sổ trượt lấy 16 mẫu trước khi suy luận
    AIoT.edgeAI.setModel(W, b, 4, 2);               // 4 đầu vào, 2 nhãn đầu ra
    AIoT.edgeAI.setNormalization(minVals, maxVals); // Cài đặt chuẩn hóa Min-Max

    // (Tùy chọn) Tinh chỉnh độ nhạy lọc nhiễu Kalman nếu cần
    // AIoT.edgeAI.setFilter(0.01f, 0.1f);
}

// ==============================================================================
// 3. LOOP: THU THẬP TÍN HIỆU & SUY LUẬN TỰ ĐỘNG
// ==============================================================================
void loop()
{
    // Duy trì kết nối mạng WiFi PnP & MQTT
    AIoT.run();

    // BƯỚC 1: Đọc cảm biến thô (ví dụ: analog ADC chân 34 hoặc từ I2C/SPI)
    float rawSensor = analogRead(34);

    // BƯỚC 2: Đẩy trực tiếp vào pipeline
    // (Hàm push() sẽ tự động lọc nhiễu Kalman và nạp vào bộ đệm vòng)
    AIoT.edgeAI.push(rawSensor);

    // BƯỚC 3: Kiểm tra khi bộ đệm đã tích lũy đủ 16 mẫu của cửa sổ trượt
    if (AIoT.edgeAI.isReady())
    {
        // Thực thi toàn bộ chuỗi: Trích xuất 4 đặc trưng -> Chuẩn hóa -> Suy luận mạng nơ-ron
        size_t label = AIoT.edgeAI.predict();

        // In bảng tóm tắt kết quả (Nhãn chiến thắng, độ tự tin %, thời gian tính toán us)
        AIoT.edgeAI.printSummary();

        // BƯỚC 4: Ra quyết định điều khiển phần cứng tại chỗ (Zero Latency)
        if (label == 1) // Phát hiện trạng thái Bất thường
        {
            AIoT.device.Relay(14).on(); // Bật rơ-le kích hoạt còi/quạt
        }
        else // Trạng thái Bình thường
        {
            AIoT.device.Relay(14).off();
        }

        // BƯỚC 5: Đẩy kết quả AI lên Cloud Dashboard qua MQTT
        AIoT.updateTelemetry("ai_label", (int)label);
        AIoT.updateTelemetry("ai_conf", AIoT.edgeAI.getWinnerConfidence() * 100.0f);
        AIoT.updateTelemetry("ai_latency_us", (int)AIoT.edgeAI.getExecutionTime());
    }

    delay(20); // Chu kỳ lấy mẫu 50Hz (20ms/mẫu)
}