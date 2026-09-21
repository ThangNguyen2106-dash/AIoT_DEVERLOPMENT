/**
 * ============================================================================
 * AIoT_LIB - Ví Dụ 05: Điện Toán AI & Toán Học Nhúng (AI_Math Academic)
 * ============================================================================
 * Chức năng:
 * - Bộ công cụ toán học thuần C++ không phụ thuộc thư viện ngoài (Zero-Dependency)
 * - Thống kê mô tả (Statistics): Mean, StdDev, RMS, Peak-to-Peak
 * - Thuật toán Welford học trực tuyến O(1) RAM và tính toán Z-Score
 * - Xử lý tín hiệu số FFT (Fast Fourier Transform) phân tích phổ tần số
 * - Thuật toán k-NN học và phân loại trực tiếp trên vi điều khiển
 * ============================================================================
 */

#include <Arduino.h>
#include <AI_Math/AI_Math.h>

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== AIoT_LIB: Vi Du 05 - AI_Math Academic ===");

    // 1. THỐNG KÊ CƠ BẢN (STATISTICS)
    float samples[] = {10.5, 12.3, 11.8, 14.2, 9.9, 13.1, 12.0, 11.5};
    size_t n = sizeof(samples) / sizeof(samples[0]);

    float meanVal = AI_Math::Statistics::mean(samples, n);
    float stdVal = AI_Math::Statistics::stdDev(samples, n);
    float rmsVal = AI_Math::Statistics::rms(samples, n);

    Serial.println("\n[1] Thong ke mo ta:");
    Serial.printf("- Mean: %.2f | StdDev: %.2f | RMS: %.2f\n", meanVal, stdVal, rmsVal);

    // 2. THUẬT TOÁN WELFORD HỌC TRỰC TUYẾN O(1) BỘ NHỚ
    Serial.println("\n[2] Welford Incremental Learning:");
    AI_Math::WelfordEstimator welford;
    for (size_t i = 0; i < n; i++)
    {
        welford.update(samples[i]);
    }
    Serial.printf("- Welford Mean: %.2f | StdDev: %.2f\n", welford.getMean(), welford.getStdDev());

    float testAnomaly = 25.0f; // Giá trị bất thường
    float zScore = welford.computeZScore(testAnomaly);
    Serial.printf("- Kiem tra Z-Score cho %.1f -> Z = %.2f sigma (Bat thuong: %s)\n",
                  testAnomaly, zScore, zScore > 3.0f ? "CO (NGUY HIEM)" : "KHONG");

    // 3. BIẾN ĐỔI FOURIER NHANH (FFT)
    Serial.println("\n[3] Fast Fourier Transform (FFT):");
    constexpr size_t FFT_SIZE = 16;
    float timeSignal[FFT_SIZE];
    for (size_t i = 0; i < FFT_SIZE; i++)
    {
        timeSignal[i] = sinf(2.0f * PI * 2.0f * (float)i / (float)FFT_SIZE);
    }
    float spectrum[FFT_SIZE / 2];
    AI_Math::FastFourierTransform::computeMagnitude(timeSignal, spectrum, FFT_SIZE);
    for (size_t i = 0; i < FFT_SIZE / 2; i++)
    {
        Serial.printf("  Bin[%u]: %.3f\n", i, spectrum[i]);
    }

    // 4. PHÂN LOẠI MÁY HỌC TẠI BIÊN k-NN (ON-DEVICE k-NN)
    Serial.println("\n[4] On-device k-NN Classifier:");
    AI_Math::OnlineKNN<2, 10, 3> knn; // 2 dac trung, toi da 10 mau, k=3

    // Huấn luyện lớp 0 (Bình thường: Rung thấp, nhiệt thấp)
    float normalSample[] = {1.0f, 25.0f};
    knn.addSample(normalSample, 0);

    // Huấn luyện lớp 1 (Lỗi động cơ: Rung cao, nhiệt cao)
    float faultSample[] = {8.5f, 75.0f};
    knn.addSample(faultSample, 1);

    // Dự đoán mẫu mới
    float query[] = {8.2f, 72.0f};
    float dist = 0.0f;
    int predicted = knn.predict(query, &dist);
    Serial.printf("- Du doan mau [8.2, 72.0] -> Lop: %d (%s) | Khoang cach: %.2f\n",
                  predicted, predicted == 1 ? "FAULT (Su co)" : "NORMAL (Binh thuong)", dist);

    Serial.println("\n=== Hoan tat vi du toan hoc. ===");
}

void loop()
{
    delay(10000);
}
