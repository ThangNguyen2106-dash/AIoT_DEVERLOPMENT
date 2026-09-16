#ifndef EDGE_AI_ANOMALY_DETECTOR_HPP
#define EDGE_AI_ANOMALY_DETECTOR_HPP

#include <Arduino.h>
#include <AI_Math/AI_Math.h>

namespace EdgeAI
{
    class AnomalyDetector
    {
    public:
        // zThreshold: ngưỡng Z-Score để coi là bất thường (thường từ 2.5 đến 3.5)
        // calibrationSamples: số lượng mẫu ban đầu dùng để tự học baseline
        AnomalyDetector(float zThreshold = 3.0f, uint32_t calibrationSamples = 50)
            : _threshold(zThreshold), _calibSamples(calibrationSamples), _isCalibrated(false)
        {
        }

        // Học mẫu dữ liệu chuẩn
        void learn(float value)
        {
            _welford.update(value);
            if (_welford.getCount() >= _calibSamples)
            {
                _isCalibrated = true;
            }
        }

        // Tự động xử lý mẫu: tự học nếu chưa đủ mẫu, sau đó trả về điểm bất thường [0.0, 1.0]
        float processSample(float value, bool &isAnomalyOut)
        {
            if (!_isCalibrated)
            {
                learn(value);
                isAnomalyOut = false;
                return 0.0f;
            }

            float z = _welford.computeZScore(value, 1.2f);
            // Chuẩn hóa điểm bất thường từ [0, threshold * 1.5] về [0.0, 1.0]
            float score = z / (_threshold * 1.5f);
            if (score > 1.0f)
                score = 1.0f;
            if (score < 0.0f)
                score = 0.0f;

            isAnomalyOut = (z >= _threshold);

            // Tự thích nghi từ từ khi mẫu an toàn bình thường (Z < 1.5σ) để theo kịp trôi dạt nhiệt độ ngày/đêm
            if (z < 1.5f)
            {
                learn(value);
            }

            return score;
        }

        // Dự đoán điểm bất thường
        float predictScore(float value) const
        {
            if (!_isCalibrated)
                return 0.0f;
            float z = _welford.computeZScore(value, 1.2f);
            float score = z / (_threshold * 1.5f);
            return (score > 1.0f) ? 1.0f : ((score < 0.0f) ? 0.0f : score);
        }

        bool isAnomaly(float value) const
        {
            if (!_isCalibrated)
                return false;
            return _welford.computeZScore(value, 1.2f) >= _threshold;
        }

        void setThreshold(float threshold) { _threshold = threshold; }
        float getThreshold() const { return _threshold; }
        bool isCalibrated() const { return _isCalibrated; }
        uint32_t getSampleCount() const { return _welford.getCount(); }
        float getBaselineMean() const { return _welford.getMean(); }
        float getBaselineStdDev() const
        {
            float s = _welford.getStdDev();
            return (s < 1.2f) ? 1.2f : s;
        }

        float getZScore(float value) const
        {
            return _welford.computeZScore(value, 1.2f);
        }

        void reset()
        {
            _welford.reset();
            _isCalibrated = false;
        }

    private:
        float _threshold;
        uint32_t _calibSamples;
        bool _isCalibrated;
        AI_Math::WelfordEstimator _welford;
    };
}

#endif /* EDGE_AI_ANOMALY_DETECTOR_HPP */

