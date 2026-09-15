#ifndef EDGE_AI_CLASSIFIER_HPP
#define EDGE_AI_CLASSIFIER_HPP

#include <Arduino.h>
#include <AI_Math/AI_Math.h>

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
}

#endif /* EDGE_AI_CLASSIFIER_HPP */

