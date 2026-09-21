#ifndef EDGE_AI_CLASSIFIER_HPP
#define EDGE_AI_CLASSIFIER_HPP

#include <Arduino.h>
#include <AI_Math/AI_Math.h>
#include "Models/CustomModelWeights.h"

namespace EdgeAI
{
    enum DeviceState
    {
        STATE_NORMAL = 0,
        STATE_WARNING = 1,
        STATE_CRITICAL = 2,
        STATE_UNKNOWN = 3
    };

    inline const char *stateToString(DeviceState state)
    {
        switch (state)
        {
        case STATE_NORMAL:
            return "NORMAL";
        case STATE_WARNING:
            return "WARNING";
        case STATE_CRITICAL:
            return "CRITICAL";
        default:
            return "UNKNOWN";
        }
    }

    // Bộ phân loại trạng thái thiết bị dựa trên đặc trưng trích xuất
    class DeviceStateClassifier
    {
    public:
        DeviceStateClassifier(float warningThresh = 0.40f, float criticalThresh = 0.66f)
            : _warnThresh(warningThresh), _critThresh(criticalThresh)
        {
        }

        DeviceState classify(float anomalyScore) const
        {
            if (anomalyScore >= _critThresh)
                return STATE_CRITICAL;
            if (anomalyScore >= _warnThresh)
                return STATE_WARNING;
            return STATE_NORMAL;
        }

        // Phân loại đa chiều từ vector đặc trưng [RMS, Peak-to-Peak, Zero-Crossings]
        DeviceState classifyFeatures(float rmsVal, float p2pVal, float maxSafeRms, float maxSafeP2P) const
        {
            float rmsRatio = rmsVal / (maxSafeRms > 0 ? maxSafeRms : 1.0f);
            float p2pRatio = p2pVal / (maxSafeP2P > 0 ? maxSafeP2P : 1.0f);
            float score = (rmsRatio * 0.6f) + (p2pRatio * 0.4f);

            if (score >= _critThresh)
                return STATE_CRITICAL;
            if (score >= _warnThresh)
                return STATE_WARNING;
            return STATE_NORMAL;
        }

        void setThresholds(float warning, float critical)
        {
            _warnThresh = warning;
            _critThresh = critical;
        }

    private:
        float _warnThresh;
        float _critThresh;
    };

    struct TinyMLResult
    {
        DeviceState predictedClass;
        float confidence;          // Xác suất Softmax cao nhất [0.0, 1.0]
        float probabilities[3];    // Xác suất Softmax của 3 nhãn: [NORMAL, WARNING, CRITICAL]
        float logits[3];           // Đầu ra thô từ tầng Dense (W * x + b)
        uint32_t inferenceTimeUs;  // Thời gian suy luận nơ-ron tính bằng micro-giây (µs)
    };

    // Bộ phân loại mạng nơ-ron TinyML tối ưu hóa cho ESP32 / ESP32-S3
    class TinyMLNeuralClassifier
    {
    public:
        TinyMLNeuralClassifier()
            : _weights(nullptr), _bias(nullptr), _numInputs(4), _numClasses(3), _hasModel(false)
        {
            loadDefaultModel();
        }

        void loadDefaultModel()
        {
            _weights = &EdgeModels::CustomModel::W[0][0];
            _bias = &EdgeModels::CustomModel::b[0];
            _numInputs = EdgeModels::CustomModel::NUM_FEATURES;
            _numClasses = EdgeModels::CustomModel::NUM_CLASSES;
            _hasModel = true;
        }

        void setCustomWeights(const float *weights, const float *bias, size_t inputs = 4, size_t classes = 3)
        {
            _weights = weights;
            _bias = bias;
            _numInputs = inputs;
            _numClasses = classes;
            _hasModel = (weights != nullptr && bias != nullptr);
        }

        bool hasModel() const { return _hasModel; }

        TinyMLResult predict(const float *features, size_t featureCount) const
        {
            TinyMLResult res;
            res.predictedClass = STATE_NORMAL;
            res.confidence = 0.0f;
            res.inferenceTimeUs = 0;
            for (int i = 0; i < 3; i++)
            {
                res.probabilities[i] = 0.0f;
                res.logits[i] = 0.0f;
            }

            if (!_hasModel || features == nullptr || featureCount == 0)
                return res;

            uint32_t startUs = micros();

            // 1. Tầng Dense (Fully-Connected): Logits = W * x + b
            AI_Math::Matrix::denseForward(_weights, features, _bias, res.logits, _numClasses, _numInputs);

            // 2. Hàm kích hoạt Softmax chuẩn hóa xác suất: exp(z_i) / sum(exp(z))
            for (size_t i = 0; i < _numClasses && i < 3; i++)
            {
                res.probabilities[i] = res.logits[i];
            }
            AI_Math::Activations::softmax(res.probabilities, _numClasses);

            // 3. Phân loại ArgMax
            size_t maxIdx = AI_Math::Activations::argmax(res.probabilities, _numClasses);
            if (maxIdx == 2)
                res.predictedClass = STATE_CRITICAL;
            else if (maxIdx == 1)
                res.predictedClass = STATE_WARNING;
            else
                res.predictedClass = STATE_NORMAL;

            res.confidence = res.probabilities[maxIdx];
            res.inferenceTimeUs = micros() - startUs;

            return res;
        }

    private:
        const float *_weights;
        const float *_bias;
        size_t _numInputs;
        size_t _numClasses;
        bool _hasModel;
    };
}

#endif /* EDGE_AI_CLASSIFIER_HPP */
