#ifndef AI_MATH_FEATURE_EXTRACTION_HPP
#define AI_MATH_FEATURE_EXTRACTION_HPP

#include <Arduino.h>
#include <math.h>

namespace AI_Math
{
    // =========================================================================
    // GIAI ĐOẠN 2: TRÍCH XUẤT ĐẶC TRƯNG & THỐNG KÊ (FEATURE EXTRACTION)
    // -------------------------------------------------------------------------
    // Mục đích:
    //   - Nén mảng N mẫu tín hiệu thời gian thực thành vector đặc trưng cô đọng.
    //   - Tính toán các chỉ số thống kê cơ bản: Mean, RMS, Peak-to-Peak, StdDev.
    //   - Chuẩn hóa các giá trị đặc trưng về dải [0.0, 1.0] (Min-Max) hoặc Z-Score.
    //
    // Đầu vào: Mảng N mẫu tín hiệu: const float *data, size_t length.
    // Đầu ra : Vector đặc trưng 4 chiều thô hoặc đã chuẩn hóa.
    // =========================================================================

    class Statistics
    {
    public:
        // ---------------------------------------------------------------------
        // 1. GIÁ TRỊ TRUNG BÌNH (MEAN)
        // Công thức: mean = (1 / N) * sum(x_i)
        // ---------------------------------------------------------------------
        static float mean(const float *data, size_t length)
        {
            if (data == nullptr || length == 0)
                return 0.0f;
            float sum = 0.0f;
            for (size_t i = 0; i < length; i++)
            {
                sum += data[i];
            }
            return sum / (float)length;
        }

        // ---------------------------------------------------------------------
        // 2. PHƯƠNG SAI MẪU (VARIANCE)
        // Công thức: var = (1 / (N - 1)) * sum((x_i - mean)^2)
        // ---------------------------------------------------------------------
        static float variance(const float *data, size_t length)
        {
            if (data == nullptr || length <= 1)
                return 0.0f;
            float m = mean(data, length);
            float sumSq = 0.0f;
            for (size_t i = 0; i < length; i++)
            {
                float diff = data[i] - m;
                sumSq += diff * diff;
            }
            return sumSq / (float)(length - 1);
        }

        // ---------------------------------------------------------------------
        // 3. ĐỘ LỆCH CHUẨN (STANDARD DEVIATION)
        // Công thức: std = sqrt(variance)
        // ---------------------------------------------------------------------
        static float stdDev(const float *data, size_t length)
        {
            return sqrtf(variance(data, length));
        }

        // ---------------------------------------------------------------------
        // 4. GIÁ TRỊ HIỆU DỤNG (ROOT MEAN SQUARE - RMS)
        // Công thức: RMS = sqrt((1 / N) * sum(x_i^2))
        // ---------------------------------------------------------------------
        static float rms(const float *data, size_t length)
        {
            if (data == nullptr || length == 0)
                return 0.0f;
            float sumSq = 0.0f;
            for (size_t i = 0; i < length; i++)
            {
                sumSq += data[i] * data[i];
            }
            return sqrtf(sumSq / (float)length);
        }

        // ---------------------------------------------------------------------
        // 5. GIÁ TRỊ CỰC TIỂU VÀ CỰC ĐẠI (MIN & MAX)
        // ---------------------------------------------------------------------
        static void minMax(const float *data, size_t length, float &minVal, float &maxVal)
        {
            if (data == nullptr || length == 0)
            {
                minVal = 0.0f;
                maxVal = 0.0f;
                return;
            }
            minVal = data[0];
            maxVal = data[0];
            for (size_t i = 1; i < length; i++)
            {
                if (data[i] < minVal)
                    minVal = data[i];
                if (data[i] > maxVal)
                    maxVal = data[i];
            }
        }

        // ---------------------------------------------------------------------
        // 6. BIÊN ĐỘ ĐỈNH - ĐÁY (PEAK-TO-PEAK)
        // Công thức: P2P = maxVal - minVal
        // ---------------------------------------------------------------------
        static float peakToPeak(const float *data, size_t length)
        {
            float minVal, maxVal;
            minMax(data, length, minVal, maxVal);
            return maxVal - minVal;
        }

        // ---------------------------------------------------------------------
        // 7. CHUẨN HÓA MIN-MAX (SCALING VỀ DẢI [0.0, 1.0])
        // Công thức: x_norm = (x - minVal) / (maxVal - minVal)
        // ---------------------------------------------------------------------
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

        // ---------------------------------------------------------------------
        // 8. CHUẨN HÓA Z-SCORE
        // Công thức: z = (x - meanVal) / stdDevVal
        // ---------------------------------------------------------------------
        static float zScore(float value, float meanVal, float stdDevVal)
        {
            if (stdDevVal < 1e-6f)
                return 0.0f;
            return (value - meanVal) / stdDevVal;
        }

        // ---------------------------------------------------------------------
        // 9. TRÍCH XUẤT VECTOR ĐẶC TRƯNG 4 CHIỀU THÔ
        // outVector4[0] = Mean
        // outVector4[1] = RMS
        // outVector4[2] = Peak-to-Peak
        // outVector4[3] = StdDev
        // ---------------------------------------------------------------------
        static void extractVector(const float *data, size_t length, float *outVector4)
        {
            if (outVector4 == nullptr)
                return;
            outVector4[0] = mean(data, length);
            outVector4[1] = rms(data, length);
            outVector4[2] = peakToPeak(data, length);
            outVector4[3] = stdDev(data, length);
        }

        // ---------------------------------------------------------------------
        // 10. CHUẨN HÓA VECTOR THEO DẢI MIN-MAX
        // outVector[i] = minMaxScale(inVector[i], minVals[i], maxVals[i])
        // ---------------------------------------------------------------------
        static void normalizeMinMax(const float *inVector, const float *minVals, const float *maxVals, float *outVector, size_t length)
        {
            if (inVector == nullptr || outVector == nullptr || minVals == nullptr || maxVals == nullptr)
                return;
            for (size_t i = 0; i < length; i++)
            {
                outVector[i] = minMaxScale(inVector[i], minVals[i], maxVals[i]);
            }
        }

        // ---------------------------------------------------------------------
        // 11. CHUẨN HÓA VECTOR THEO Z-SCORE
        // outVector[i] = zScore(inVector[i], meanVals[i], stdDevVals[i])
        // ---------------------------------------------------------------------
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

    // Alias: Sử dụng FeatureExtraction hoặc Statistics đều như nhau
    using FeatureExtraction = Statistics;
}

#endif /* AI_MATH_FEATURE_EXTRACTION_HPP */