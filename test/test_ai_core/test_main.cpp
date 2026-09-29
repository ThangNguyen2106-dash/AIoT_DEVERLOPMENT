/**
 * ==============================================================================
 *                     AIoT_LIB AUTOMATED UNIT TEST SUITE
 * ==============================================================================
 * Kiểm thử tự động toàn bộ phần CORE Toán học & Thuật toán AI:
 *   1. Tiền xử lý tín hiệu: KalmanFilter & CircularBuffer (Sliding Window FIFO)
 *   2. Trích xuất đặc trưng: Welford Feature Extractor (Mean, RMS, P2P, StdDev)
 *   3. Chuẩn hóa Vector: Min-Max & Z-Score Normalization
 *   4. Động cơ Mạng Nơ-ron: NeuralEngine (Feedforward, Softmax, Sigmoid)
 *   5. Toàn vẹn Pipeline & Mã băm CRC32: Checksum & Fault tolerance
 * ==============================================================================
 */

#include <Arduino.h>
#include <unity.h>
#include <AIoT.h>

void setUp(void)
{
    // Setup chạy trước mỗi test
}

void tearDown(void)
{
    // Cleanup chạy sau mỗi test
}

// ==============================================================================
// 1. UNIT TEST: TIỀN XỬ LÝ TÍN HIỆU (KALMAN & CIRCULAR BUFFER)
// ==============================================================================

void test_kalman_filter_convergence(void)
{
    AI_Math::KalmanFilter kf(0.01f, 0.1f, 1.0f, 0.0f);
    float noisySignal[] = {48.5f, 52.1f, 49.0f, 51.5f, 50.2f, 49.8f, 50.5f, 49.9f};
    float clean = 0.0f;

    for (size_t i = 0; i < 8; i++)
    {
        clean = kf.update(noisySignal[i]);
    }

    // Sau khi lọc nhiễu, giá trị hội tụ về 50.0 với sai số < 1.0
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 50.0f, clean);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 50.0f, kf.getCleanValue());
}

void test_circular_buffer_fifo(void)
{
    AI_Math::CircularBuffer<4> buf;
    TEST_ASSERT_EQUAL_UINT32(0, buf.size());
    TEST_ASSERT_FALSE(buf.isFull());

    buf.push(10.0f);
    buf.push(20.0f);
    TEST_ASSERT_EQUAL_UINT32(2, buf.size());
    TEST_ASSERT_EQUAL_FLOAT(10.0f, buf.get(0));
    TEST_ASSERT_EQUAL_FLOAT(20.0f, buf.get(1));

    buf.push(30.0f);
    buf.push(40.0f);
    TEST_ASSERT_TRUE(buf.isFull());
    TEST_ASSERT_EQUAL_UINT32(4, buf.size());

    // Đẩy mẫu thứ 5: mẫu 10.0 cũ nhất phải tự động bị hủy (FIFO sliding window)
    buf.push(50.0f);
    TEST_ASSERT_EQUAL_UINT32(4, buf.size());
    TEST_ASSERT_EQUAL_FLOAT(20.0f, buf.get(0)); // Cũ nhất giờ là 20.0
    TEST_ASSERT_EQUAL_FLOAT(50.0f, buf.get(3)); // Mới nhất là 50.0

    float out[4];
    buf.toArray(out);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, out[0]);
    TEST_ASSERT_EQUAL_FLOAT(30.0f, out[1]);
    TEST_ASSERT_EQUAL_FLOAT(40.0f, out[2]);
    TEST_ASSERT_EQUAL_FLOAT(50.0f, out[3]);

    buf.clear();
    TEST_ASSERT_EQUAL_UINT32(0, buf.size());
    TEST_ASSERT_FALSE(buf.isFull());
}

// ==============================================================================
// 2. UNIT TEST: TRÍCH XUẤT ĐẶC TRƯNG THỐNG KÊ (WELFORD ALGORITHM)
// ==============================================================================

void test_feature_extractor_welford(void)
{
    // Chuỗi kiểm thử chuẩn: [10.0, 20.0, 30.0, 40.0]
    // 1. Mean = 25.0
    // 2. RMS = sqrt(750) ≈ 27.386127
    // 3. P2P = 40.0 - 10.0 = 30.0
    // 4. Sample Variance = 500 / 3 => StdDev ≈ 12.909944

    float samples[] = {10.0f, 20.0f, 30.0f, 40.0f};
    float features[4] = {0.0f};

    AI_Math::FeatureExtractor::extract(samples, 4, features);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.000f, features[0]); // Mean
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 27.386f, features[1]); // RMS
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 30.000f, features[2]); // P2P
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.910f, features[3]); // StdDev
}

void test_feature_extractor_empty_buffer(void)
{
    float features[4] = {99.0f, 99.0f, 99.0f, 99.0f};
    AI_Math::FeatureExtractor::extract(nullptr, 0, features);

    // Không bị chia cho 0 và reset về 0.0
    TEST_ASSERT_EQUAL_FLOAT(0.0f, features[0]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, features[1]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, features[2]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, features[3]);
}

// ==============================================================================
// 3. UNIT TEST: CHUẨN HÓA DỮ LIỆU (MIN-MAX & Z-SCORE)
// ==============================================================================

void test_min_max_scaling(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5f, AI_Math::FeatureExtractor::minMaxScale(5.0f, 0.0f, 10.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, AI_Math::FeatureExtractor::minMaxScale(-5.0f, 0.0f, 10.0f));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, AI_Math::FeatureExtractor::minMaxScale(15.0f, 0.0f, 10.0f));
    // Chống chia cho 0 khi max == min
    TEST_ASSERT_EQUAL_FLOAT(0.0f, AI_Math::FeatureExtractor::minMaxScale(5.0f, 5.0f, 5.0f));
}

void test_z_score_scaling(void)
{
    // z = (val - mean) / stdDev = (35.0 - 25.0) / 5.0 = 2.0
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.0f, AI_Math::FeatureExtractor::zScore(35.0f, 25.0f, 5.0f));
    // Chống chia cho 0 khi stdDev == 0
    TEST_ASSERT_EQUAL_FLOAT(0.0f, AI_Math::FeatureExtractor::zScore(35.0f, 25.0f, 0.0f));
}

// ==============================================================================
// 4. UNIT TEST: THỰC THI MẠNG NƠ-RON (NEURAL ENGINE)
// ==============================================================================

void test_neural_engine_softmax_and_argmax(void)
{
    // 2 đầu vào, 3 nhãn Softmax
    const float W[6] = {
        1.0f, 0.0f,
        0.0f, 2.0f,
        -1.0f, -1.0f};
    const float b[3] = {0.0f, 0.0f, 0.0f};

    // Vector đầu vào: x = [0.0, 3.0]
    // Logit nhãn 1 = 6.0 => Nhãn 1 phải thắng áp đảo (> 95%)
    const float x[2] = {0.0f, 3.0f};
    float probs[3] = {0.0f};
    uint32_t execTime = 0;

    size_t winner = AI_Math::NeuralEngine::predict(
        x, 2, 3, 0,
        W, b,
        probs, nullptr,
        &execTime);

    TEST_ASSERT_EQUAL_UINT32(1, winner);
    TEST_ASSERT_TRUE(probs[1] > 0.95f);

    // Tính chất Softmax: Tổng xác suất các nhãn PHẢI = 1.0 (100%)
    float sumProb = probs[0] + probs[1] + probs[2];
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, sumProb);
    TEST_ASSERT_TRUE(execTime > 0);
}

void test_neural_engine_sigmoid_independent_commands(void)
{
    const float W[4] = {
        1.0f, 0.0f,
        -2.0f, 0.0f};
    const float b[2] = {0.0f, 0.0f};

    // x = [0.0, 0.0] => logit = 0.0 => sigmoid(0) = 0.5 (50%)
    const float x_zero[2] = {0.0f, 0.0f};
    float cmds[2] = {0.0f};

    AI_Math::NeuralEngine::predict(
        x_zero, 2, 0, 2,
        W, b,
        nullptr, cmds,
        nullptr);

    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5f, cmds[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5f, cmds[1]);

    // x = [5.0, 0.0] => cmd0 logit = 5.0 (> 0.9), cmd1 logit = -10.0 (< 0.01)
    const float x_pos[2] = {5.0f, 0.0f};
    AI_Math::NeuralEngine::predict(
        x_pos, 2, 0, 2,
        W, b,
        nullptr, cmds,
        nullptr);

    TEST_ASSERT_TRUE(cmds[0] > 0.90f);
    TEST_ASSERT_TRUE(cmds[1] < 0.01f);
}

// ==============================================================================
// 5. UNIT TEST: KIỂM TRA MÃ TOÀN VẸN (CRC32 CHECKSUM)
// ==============================================================================

void test_edgeai_crc32_integrity(void)
{
    float testWeights[] = {1.23f, -4.56f, 7.89f, 0.001f};
    uint32_t crc1 = EdgeAI::calculateCRC(testWeights, 4);
    uint32_t crc2 = EdgeAI::calculateCRC(testWeights, 4);

    TEST_ASSERT_EQUAL_UINT32(crc1, crc2);
    TEST_ASSERT_NOT_EQUAL(0, crc1);

    testWeights[0] += 0.0001f;
    uint32_t crc_corrupted = EdgeAI::calculateCRC(testWeights, 4);
    TEST_ASSERT_NOT_EQUAL(crc1, crc_corrupted);
}

// ==============================================================================
// 6. UNIT TEST: KỊCH BẢN CẢM BIẾN CHẾT / ĐỨT DÂY (ZERO VARIANCE RESILIENCE)
// ==============================================================================

void test_sensor_fault_zero_variance_resilience(void)
{
    // Giả lập cảm biến bị đứt dây: Toàn bộ 16 mẫu đều là 0.0f
    AI_Math::CircularBuffer<16> deadBuf;
    for (size_t i = 0; i < 16; i++)
    {
        deadBuf.push(0.0f);
    }

    float deadSamples[16];
    deadBuf.toArray(deadSamples);

    float featVec[4] = {-1.0f, -1.0f, -1.0f, -1.0f};
    AI_Math::FeatureExtractor::extract(deadSamples, 16, featVec);

    // Xác nhận: Mean, RMS, P2P, StdDev đều là 0.0f, KHÔNG ĐƯỢC sinh ra NaN hay Inf
    TEST_ASSERT_EQUAL_FLOAT(0.0f, featVec[0]); // Mean
    TEST_ASSERT_EQUAL_FLOAT(0.0f, featVec[1]); // RMS
    TEST_ASSERT_EQUAL_FLOAT(0.0f, featVec[2]); // P2P
    TEST_ASSERT_EQUAL_FLOAT(0.0f, featVec[3]); // StdDev
    TEST_ASSERT_FALSE(isnan(featVec[3]));

    // Thử nghiệm chuẩn hóa Z-Score với stdDev = 0 (bảo vệ chống chia cho 0)
    float normOut[4] = {-99.0f, -99.0f, -99.0f, -99.0f};
    float meanZero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float stdZero[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // stdDev = 0 (Nguy cơ chia cho 0)

    AI_Math::Normalization::normalizeZScore(featVec, meanZero, stdZero, normOut, 4);

    // Nhờ chốt chặn an toàn (stdDevVal < 1e-6f), kết quả phải là 0.0f an toàn thay vì NaN
    for (size_t i = 0; i < 4; i++)
    {
        TEST_ASSERT_EQUAL_FLOAT(0.0f, normOut[i]);
        TEST_ASSERT_FALSE(isnan(normOut[i]));
        TEST_ASSERT_FALSE(isinf(normOut[i]));
    }
}

// ==============================================================================
// 7. UNIT TEST: KIỂM TRA RÒ RỈ BỘ NHỚ THEO THỜI GIAN (ZERO MEMORY LEAK)
// ==============================================================================

void test_long_term_zero_memory_leak(void)
{
    EdgeAI ai;
    ai.begin(16, 1);

    const float W[4] = {1.0f, 0.5f, -0.5f, 0.2f};
    const float b[1] = {0.1f};
    ai.setModel(W, b, 4, 1, 0);

    // Warm up pipeline
    for (size_t i = 0; i < 16; i++)
    {
        ai.push(0, 50.0f);
    }
    ai.predict();

#if defined(ESP32)
    uint32_t initialHeap = ESP.getFreeHeap();

    // Chạy 1.000 chu kỳ suy luận liên tục
    for (size_t iter = 0; iter < 1000; iter++)
    {
        ai.push(0, 50.0f + (float)(iter % 5));
        ai.predict();
        float conf = ai.getWinnerConfidence();
        (void)conf;
    }

    uint32_t finalHeap = ESP.getFreeHeap();

    // Khẳng định: Dung lượng Free Heap trước và sau 1.000 lần chạy phải hoàn toàn bằng nhau
    TEST_ASSERT_EQUAL_UINT32(initialHeap, finalHeap);
#else
    for (size_t iter = 0; iter < 1000; iter++)
    {
        ai.push(0, 50.0f + (float)(iter % 5));
        size_t res = ai.predict();
        TEST_ASSERT_EQUAL_UINT32(0, res);
    }
#endif
}

// ==============================================================================
// TEST RUNNER MAIN
// ==============================================================================

void run_all_unit_tests(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_kalman_filter_convergence);
    RUN_TEST(test_circular_buffer_fifo);
    RUN_TEST(test_feature_extractor_welford);
    RUN_TEST(test_feature_extractor_empty_buffer);
    RUN_TEST(test_min_max_scaling);
    RUN_TEST(test_z_score_scaling);
    RUN_TEST(test_neural_engine_softmax_and_argmax);
    RUN_TEST(test_neural_engine_sigmoid_independent_commands);
    RUN_TEST(test_edgeai_crc32_integrity);
    RUN_TEST(test_sensor_fault_zero_variance_resilience);
    RUN_TEST(test_long_term_zero_memory_leak);

    UNITY_END();
}

void setup()
{
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n\n=======================================================");
    Serial.println("         KHOI CHAY BO UNIT TEST AIoT PLATFORM          ");
    Serial.println("=======================================================");

    run_all_unit_tests();
}

void loop()
{
    delay(1000);
}
