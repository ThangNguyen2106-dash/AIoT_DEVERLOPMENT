#ifndef AI_MATH_FEATURE_EXTRACTION_HPP
#define AI_MATH_FEATURE_EXTRACTION_HPP

#include <Arduino.h>
#include <math.h>

namespace AI_Math
{
    // =========================================================================
    // GIAI ĐOẠN 2: TRÍCH XUẤT ĐẶC TRƯNG ĐỒNG BỘ (STANDARD FEATURE EXTRACTION)
    // -------------------------------------------------------------------------
    // Mục đích:
    //   - Trích xuất đồng thời và cố định 4 chỉ số (Mean, RMS, P2P, StdDev)
    //     cho bất kỳ chuỗi cảm biến nào được đưa vào.
    //   - Sử dụng thuật toán Welford để tính toán Mean và StdDev song song,
    //     tiết kiệm 50% số phép tính so với cách tính truyền thống.
    //   - Chuẩn hóa vector đặc trưng 4 chiều về dải (Min Max) hoặc Z Score
    // =========================================================================

    class FeatureExtractor
    {
    public:
        /**
         * @brief Hàm trích xuất cố định 4 đặc trưng thống kê từ chuỗi mẫu sạch
         * @param cleanSamples Con trỏ trỏ tới mảng dữ liệu sạch (đã qua bộ lọc Kalman)
         * @param sampleCount Tổng số mẫu thực tế đang lưu trong bộ đệm vòng của cảm biến
         * @param outputVector Con trỏ trỏ đến vị trí mảng phẳng (Flatten Vector) đầu vào của mạng AI
         *
         * @note Hàm này sẽ tự động ghi liên tiếp 4 số float vào outputVector theo thứ tự vĩnh viễn:
         *       outputVector[0] = Mean
         *       outputVector[1] = RMS
         *       outputVector[2] = P2P
         *       outputVector[3] = StdDev
         */

        // Trích xuất đặc trưng của đối tượng thành một Vector thô
        static inline void extract(const float *cleanSamples, size_t sampleCount, float *outputVector)
        {
            if (sampleCount == 0)
            {
                outputVector[0] = 0.0f;
                outputVector[1] = 0.0f;
                outputVector[2] = 0.0f;
                outputVector[3] = 0.0f;
                return;
            }

            // --- 1. Thuật toán Welford tính toán cuốn chiếu để tìm Mean và Variance ---
            float runningMean = 0.0f;
            float M2 = 0.0f;

            // --- 2. Khởi tạo các biến tích lũy cho RMS và P2P ---
            float sumSquares = 0.0f;
            float maxVal = cleanSamples[0];
            float minVal = cleanSamples[0];

            for (size_t k = 1; k <= sampleCount; k++)
            {
                float val = cleanSamples[k - 1];

                // Logic tìm cực trị cho P2P
                if (val > maxVal)
                    maxVal = val;
                if (val < minVal)
                    minVal = val;

                // Tích lũy bình phương cho RMS
                sumSquares += (val * val);

                // Logic Welford cho Mean & StdDev
                float delta = val - runningMean;
                runningMean += delta / k;
                float delta2 = val - runningMean;
                M2 += delta * delta2;
            }
            // --- 3. Đổ dữ liệu phẳng nối đuôi trực tiếp vào Vector đầu vào mạng AI ---
            outputVector[0] = runningMean;                    // Chỉ số 1: Mean
            outputVector[1] = sqrt(sumSquares / sampleCount); // Chỉ số 2: RMS
            outputVector[2] = maxVal - minVal;                // Chỉ số 3: P2P
            // Chỉ số 4: StdDev
            float variance = (sampleCount > 1) ? (M2 / (sampleCount - 1)) : 0.0f;
            outputVector[3] = sqrt(variance);
        }

        // Chuẩn hóa Vector đặc trưng theo chuẩn hóa Min - Max (Công thức: x_norm = (x - minVal) / (maxVal - minVal))
        static float minMaxScale(float value, float minVal, float maxVal)
        {
            if (fabsf(maxVal - minVal) < 1e-6f)
                return 0.0f;
            float scaled = (value - minVal) / (maxVal - minVal);
            if (scaled < 0.0f)
                scaled = 0.0f;
            if (scaled > 1.0f)
                scaled = 1.0f;
            return scaled;
        }
        static void normalizeMinMax(const float *inVector, const float *minVals, const float *maxVals, float *outVector, size_t length)
        {
            if (inVector == nullptr || outVector == nullptr || minVals == nullptr || maxVals == nullptr)
                return;
            for (size_t i = 0; i < length; i++)
            {
                outVector[i] = minMaxScale(inVector[i], minVals[i], maxVals[i]);
            }
        }

        // Chuẩn hóa Vector đặc trưng theo chuẩn hóa Z-Score () (Công thức: z = (x - meanVal) / stdDevVal)
        static float zScore(float value, float meanVal, float stdDevVal)
        {
            if (stdDevVal < 1e-6f)
                return 0.0f;
            return (value - meanVal) / stdDevVal;
        }
        static void normalizeZScore(const float *inVector, const float *meanVals, const float *stdDevVals, float *outVector, size_t length)
        {
            if (inVector == nullptr || outVector == nullptr || meanVals == nullptr || stdDevVals == nullptr)
                return;
            for (size_t i = 0; i < length; i++)
            {
                outVector[i] = zScore(inVector[i], meanVals[i], stdDevVals[i]);
            }
        }
    };

}
AI_Math::FeatureExtractor featureExtractor;
#endif /* AI_MATH_FEATURE_EXTRACTION_HPP */