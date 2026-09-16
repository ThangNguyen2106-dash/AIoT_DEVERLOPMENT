#ifndef EDGE_AI_HPP
#define EDGE_AI_HPP

#include <Arduino.h>
#include <AI_Math/AI_Math.h>
#include "AnomalyDetector.hpp"
#include "Classifier.hpp"

namespace EdgeAI
{
    struct InferenceResult
    {
        float score;           // Điểm số bất thường [0.0, 1.0]
        DeviceState state;     // Trạng thái: STATE_NORMAL, STATE_WARNING, STATE_CRITICAL
        bool isEmergency;      // Cần xử lý khẩn cấp tại chỗ (ngắt rơ-le)
        const char *label;     // Chuỗi trạng thái ("NORMAL", "WARNING", "CRITICAL")
        float zScore;          // Độ lệch chuẩn Z-Score tức thời
        float p2p;             // Biên độ rung lắc đỉnh-đáy (Peak-to-Peak)
        float rms;             // Năng lượng hiệu dụng
        float stdDev;          // Độ lệch chuẩn cửa sổ trượt
        const char *reason;    // Nguyên nhân dị thường ("Ổn định", "Lệch Z-Score", "Sốc nhiệt", "Rung lắc P2P")
    };

    class Engine
    {
    public:
        Engine() : _slidingWindow(), _detector(), _classifier(), _anomalyStreak(0), _lastSample(0.0f) {}

        void begin(float zThreshold = 3.0f, uint32_t calibrationSamples = 30)
        {
            calibrate(calibrationSamples, zThreshold);
        }

        // 1. DẠY LẠI TỪ ĐẦU (Calibrate): Học lại Baseline môi trường mới với N mẫu
        void calibrate(uint32_t calibrationSamples = 30, float zThreshold = 3.0f)
        {
            _detector.calibrate(calibrationSamples, zThreshold);
            _slidingWindow.clear();
            _anomalyStreak = 0;
            _lastSample = 0.0f;
        }

        // 2. DẠY MẪU CHUẨN TRỰC TIẾP (Online Teaching): Nạp một mẫu được xác nhận là bình thường
        void teachNormal(float sample)
        {
            _detector.learn(sample);
        }

        // 3. ĐIỀU CHỈNH ĐỘ NHẠY (Sensitivity Tuning):
        // 2.2f: Nhạy cao (High) - Cảnh báo rất sớm
        // 3.0f: Tiêu chuẩn (Medium) - Chuẩn 3-sigma công nghiệp
        // 3.8f: Thấp (Low) - Chống báo động giả tối đa trong môi trường nhiều nhiễu
        void setSensitivity(float zThreshold)
        {
            _detector.setThreshold(zThreshold);
        }

        // Đưa một mẫu cảm biến vào và thực thi suy luận tại chỗ (tích hợp Z-Score, Động học & Lọc nhiễu)
        InferenceResult process(float sample)
        {
            _slidingWindow.push(sample);

            bool isAnomalyRaw = false;
            float score = _detector.processSample(sample, isAnomalyRaw);
            float currentZ = _detector.getZScore(sample);

            float meanVal = 0, rmsVal = 0, p2pVal = 0, stdDevVal = 0;
            extractFeatures(meanVal, rmsVal, p2pVal, stdDevVal);

            float tempDelta = (_lastSample > 0.0f) ? fabsf(sample - _lastSample) : 0.0f;
            _lastSample = sample;

            // Nhận diện đa hình thái dị thường:
            bool isZScoreCritical = (currentZ >= _detector.getThreshold());
            bool isDynamicShock = (tempDelta >= 4.0f && currentZ >= 2.0f);
            bool isThermalInstability = (p2pVal >= 6.0f && stdDevVal >= 2.0f && currentZ >= 1.8f);

            bool isAnomaly = (isZScoreCritical || isDynamicShock || isThermalInstability);

            const char *reason = "Ổn định bình thường";
            if (isDynamicShock)
                reason = "Sốc nhiệt đột ngột (Thermal Shock)";
            else if (isThermalInstability)
                reason = "Rung lắc nhiệt độ dữ dội (Instability)";
            else if (isZScoreCritical)
                reason = "Đột biến lệch chuẩn (Z-Score Critical)";
            else if (currentZ >= 1.5f)
                reason = "Chớm lệch baseline (Warning)";

            // Bộ lọc chống nhiễu / chống xung nhảy số đơn lẻ (yêu cầu 2 mẫu liên tiếp)
            bool isRealCritical = false;
            if (isAnomaly)
            {
                _anomalyStreak++;
                if (_anomalyStreak >= 2)
                {
                    isRealCritical = true;
                }
            }
            else
            {
                if (_anomalyStreak > 0)
                    _anomalyStreak--;
            }

            DeviceState state = STATE_NORMAL;
            if (isRealCritical)
            {
                state = STATE_CRITICAL;
            }
            else if (currentZ >= 1.5f || isAnomaly || _anomalyStreak == 1)
            {
                state = STATE_WARNING;
            }

            InferenceResult res;
            res.score = score;
            res.state = state;
            res.isEmergency = (state == STATE_CRITICAL);
            res.label = stateToString(state);
            res.zScore = currentZ;
            res.p2p = p2pVal;
            res.rms = rmsVal;
            res.stdDev = stdDevVal;
            res.reason = reason;
            return res;
        }

        // Trích xuất vector đặc trưng từ cửa sổ mẫu hiện tại
        void extractFeatures(float &meanVal, float &rmsVal, float &p2pVal, float &stdDevVal) const
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

            meanVal = AI_Math::Statistics::mean(data, count);
            rmsVal = AI_Math::Statistics::rms(data, count);
            p2pVal = AI_Math::Statistics::peakToPeak(data, count);
            stdDevVal = AI_Math::Statistics::stdDev(data, count);
        }

        // Suy luận mô hình nơ-ron tổng quát: Cho phép nạp bất kỳ ma trận trọng số W, b của người dùng
        int predict(const float *features, const float *W, const float *b, size_t numClasses, size_t numFeatures, float &confidenceOut) const
        {
            if (numClasses == 0 || numFeatures == 0 || W == nullptr || b == nullptr || features == nullptr)
            {
                confidenceOut = 0.0f;
                return -1;
            }

            float logits[16];
            size_t classes = (numClasses > 16) ? 16 : numClasses;

            AI_Math::Matrix::denseForward(W, features, b, logits, classes, numFeatures);
            AI_Math::Activations::softmax(logits, classes);
            size_t bestClass = AI_Math::Activations::argmax(logits, classes);
            confidenceOut = logits[bestClass];
            return (int)bestClass;
        }

        // Suy luận với các đặc trưng tự động trích xuất từ cửa sổ trượt
        int predict(const float *W, const float *b, size_t numClasses, size_t numFeatures, float &confidenceOut) const
        {
            float feat[4];
            extractFeatures(feat[0], feat[1], feat[2], feat[3]);
            return predict(feat, W, b, numClasses, (numFeatures < 4) ? numFeatures : 4, confidenceOut);
        }

        AnomalyDetector &getDetector() { return _detector; }
        DeviceStateClassifier &getClassifier() { return _classifier; }

    private:
        AI_Math::SlidingWindow<64> _slidingWindow;
        AnomalyDetector _detector;
        DeviceStateClassifier _classifier;
        int _anomalyStreak;
        float _lastSample;
    };
}

#endif /* EDGE_AI_HPP */

