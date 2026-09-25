#ifndef EDGE_AI_HPP
#define EDGE_AI_HPP

#include <Arduino.h>
#include <AI_Math/AI_Math.h>

namespace EdgeAI
{
    // =========================================================================
    // CẤU TRÚC VECTOR ĐẶC TRƯNG SAU KHI ĐÃ CHUẨN HÓA (FEATURE VECTOR)
    // -------------------------------------------------------------------------
    // Mặc định nén 4 chiều: [0]: Mean, [1]: RMS, [2]: Peak-to-Peak, [3]: StdDev
    // Dữ liệu đã được chuẩn hóa về dải [0.0, 1.0] hoặc chuẩn hóa Z-Score.
    // =========================================================================
    struct FeatureVector
    {
        static constexpr size_t NUM_FEATURES = 4;
        float values[NUM_FEATURES];

        FeatureVector()
        {
            for (size_t i = 0; i < NUM_FEATURES; i++)
            {
                values[i] = 0.0f;
            }
        }

        FeatureVector(float m, float r, float p, float s)
        {
            values[0] = m;
            values[1] = r;
            values[2] = p;
            values[3] = s;
        }

        float mean() const { return values[0]; }
        float rms() const { return values[1]; }
        float p2p() const { return values[2]; }
        float stdDev() const { return values[3]; }

        const float *data() const { return values; }
        float *data() { return values; }
        size_t size() const { return NUM_FEATURES; }

        float operator[](size_t idx) const { return (idx < NUM_FEATURES) ? values[idx] : 0.0f; }
        float &operator[](size_t idx) { return values[idx]; }
    };

    // Cấu hình dải chuẩn hóa Min-Max cho các đặc trưng
    struct NormalizationConfig
    {
        float minVals[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        float maxVals[4] = {100.0f, 100.0f, 50.0f, 25.0f};
    };

    // =========================================================================
    // EDGE AI ENGINE: THU THẬP TÍN HIỆU & XUẤT VECTOR ĐẶC TRƯNG CHUẨN HÓA
    // =========================================================================
    class Engine
    {
    public:
        Engine() : _slidingWindow() {}

        // Đẩy 1 mẫu tín hiệu mới vào cửa sổ trượt
        void push(float sample)
        {
            _slidingWindow.push(sample);
        }

        void clear()
        {
            _slidingWindow.clear();
        }

        size_t sampleCount() const
        {
            return _slidingWindow.size();
        }

        bool isReady() const
        {
            return _slidingWindow.size() >= 4;
        }

        void setNormalizationConfig(const NormalizationConfig &config)
        {
            _config = config;
        }

        void setMinMaxRanges(const float *minVals, const float *maxVals)
        {
            if (minVals != nullptr && maxVals != nullptr)
            {
                for (size_t i = 0; i < 4; i++)
                {
                    _config.minVals[i] = minVals[i];
                    _config.maxVals[i] = maxVals[i];
                }
            }
        }

        // Trích xuất 4 đặc trưng thô: Mean, RMS, Peak-to-Peak, StdDev
        void extractRawFeatures(float &meanVal, float &rmsVal, float &p2pVal, float &stdDevVal) const
        {
            size_t n = _slidingWindow.size();
            if (n == 0)
            {
                meanVal = rmsVal = p2pVal = stdDevVal = 0.0f;
                return;
            }
            float data[64];
            size_t count = (n > 64) ? 64 : n;
            _slidingWindow.toArray(data);

            meanVal = AI_Math::FeatureExtraction::mean(data, count);
            rmsVal = AI_Math::FeatureExtraction::rms(data, count);
            p2pVal = AI_Math::FeatureExtraction::peakToPeak(data, count);
            stdDevVal = AI_Math::FeatureExtraction::stdDev(data, count);
        }

        // Trích xuất vector đặc trưng sau khi đã chuẩn hóa (Min-Max Scaling [0.0, 1.0])
        FeatureVector extractNormalizedVector() const
        {
            float rawMean = 0, rawRMS = 0, rawP2P = 0, rawStdDev = 0;
            extractRawFeatures(rawMean, rawRMS, rawP2P, rawStdDev);

            FeatureVector vec;
            vec.values[0] = AI_Math::FeatureExtraction::minMaxScale(rawMean, _config.minVals[0], _config.maxVals[0]);
            vec.values[1] = AI_Math::FeatureExtraction::minMaxScale(rawRMS, _config.minVals[1], _config.maxVals[1]);
            vec.values[2] = AI_Math::FeatureExtraction::minMaxScale(rawP2P, _config.minVals[2], _config.maxVals[2]);
            vec.values[3] = AI_Math::FeatureExtraction::minMaxScale(rawStdDev, _config.minVals[3], _config.maxVals[3]);
            return vec;
        }

        // Đẩy mẫu tín hiệu và trả về vector sau khi đã chuẩn hóa
        FeatureVector process(float sample)
        {
            push(sample);
            return extractNormalizedVector();
        }

    private:
        AI_Math::SlidingWindow<64> _slidingWindow;
        NormalizationConfig _config;
    };
}

#endif /* EDGE_AI_HPP */
