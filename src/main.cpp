#include <Arduino.h>
#include <AIoT.h>

constexpr size_t WINDOW_SIZE = 16;
constexpr size_t MIN_SAMPLES = 8;

// Giai đoạn 1: Lập bộ lọc và Cửa sổ lưu trữ chuỗi lịch sử ngắn hạn
AI_Math::KalmanFilter1D kalman(0.01f, 0.5f);
AI_Math::SlidingWindow<WINDOW_SIZE> window;

// Giai đoạn 2: Cấu hình dải chuẩn hóa dữ liệu tức thời [Mean, RMS, P2P, StdDev]
const float minRanges[4] = {0.0f, 0.0f, 0.0f, 0.0f};
const float maxRanges[4] = {50.0f, 50.0f, 20.0f, 10.0f};

// Giai đoạn 3: Custom bộ Trọng số W [3 lớp đầu ra x 4 đặc trưng đầu vào] và Bias b
const float W[3 * 4] = {
    -0.50f, -0.40f, -1.20f, -1.50f, // Nhãn lớp 0 (Bình thường - nhận ưu tiên cộng điểm)
    0.20f, 0.30f, 0.80f, 0.70f,     // Nhãn lớp 1 (Cảnh báo biến động nhẹ)
    0.80f, 0.90f, 2.10f, 2.40f      // Nhãn lớp 2 (Nguy hiểm / Sự cố phòng học)
};
const float b[3] = {1.0f, -0.2f, -1.5f};
const float normalCentroids[4] = {0.20f, 0.20f, 0.05f, 0.02f}; // Tâm điểm trạng thái xanh lý tưởng
constexpr float ANOMALY_THRESHOLD = 0.35f;

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ;
    Serial.println(F("============================================="));
    Serial.println(F("AIoT_LIB Core Engine: Khởi động Giai đoạn 3 thành công!"));
    // Do sử dụng hàm static tối ưu vùng nhớ, ta không cần nạp bộ cài đặt đối tượng tại đây nữa [1]
}

void loop()
{
    // Giả lập đọc chỉ số cảm biến vật lý (ví dụ: Nhiệt độ phòng học tức thời)
    float rawSample = 10.0f + ((float)random(-15, 16) / 10.0f);

    // GIAI ĐOẠN 1: Tiền xử lý tín hiệu sạch qua Kalman và lưu đệm vòng
    float cleanSample = kalman.update(rawSample);
    window.push(cleanSample);

    if (window.size() < MIN_SAMPLES)
    {
        delay(100);
        return;
    }

    // GIAI ĐOẠN 2: Trích xuất Vector đặc trưng chuỗi & Chuẩn hóa mảng phẳng
    float dataBuffer[WINDOW_SIZE];
    window.toArray(dataBuffer);

    float rawVector[4];
    float normalizedVector[4];

    // Gọi lớp Statistics thực hiện xử lý trích xuất đặc trưng
    AI_Math::Statistics::extractVector(dataBuffer, window.size(), rawVector);
    AI_Math::Statistics::normalizeMinMax(rawVector, minRanges, maxRanges, normalizedVector, 4);

    // GIAI ĐOẠN 3: Đưa vector đa chiều vào ma trận trọng số Custom để ép tìm Kẻ Vô Địch
    // Truyền tham số false cuối để áp dụng Sigmoid hàng loạt phục vụ đa kịch bản Cmd
    AI_Math::InferenceResult result = AI_Math::NeuralClassifier::predict(
        normalizedVector,
        W,
        b,
        3,    // Số lớp đầu ra
        4,    // Số chiều đặc trưng đầu vào
        false // useSoftmax = false -> Kích hoạt hàm Sigmoid
    );

    // Đo lường độ dị biệt hệ thống thông qua K-Means tĩnh
    float anomalyScore = 0.0f;
    bool hasAnomaly = AI_Math::KMeansAnomalyDetector::isAnomaly(
        normalizedVector,
        normalCentroids,
        1,
        4,
        ANOMALY_THRESHOLD,
        &anomalyScore);

    // Xuất log giám sát thời gian thực lên cổng Serial Monitor
    Serial.printf("Lớp dự đoán: %u | Độ tin cậy: %.1f%% | Khoảng cách K-Means: %.3f | Thời gian: %u us | Bất thường: %s\n",
                  result.predictedClass,
                  result.confidence * 100.0f,
                  anomalyScore,
                  result.executionTimeUs,
                  hasAnomaly ? "CÓ (Hệ thống lỗi/Phá hoại)" : "KHÔNG (An toàn)");

    delay(1500);
}