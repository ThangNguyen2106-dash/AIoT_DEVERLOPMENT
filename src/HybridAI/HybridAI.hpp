#ifndef HYBRID_AI_HPP
#define HYBRID_AI_HPP

#include <Arduino.h>
#include <AI_Math/AI_Math.h>
#include <EdgeAI/EdgeAI.h>
#include <CloudAI/CloudAI.h>

// --- Hybrid AI Architecture: Types, Policy, Orchestrator ---
#include "Types.hpp"
#include "PolicyEngine.hpp"
#include "AIOrchestrator.hpp"

namespace HybridAI
{

    class Engine
    {
    public:
        explicit Engine(const PolicyConfig &config = PolicyConfig{})
            : _orchestrator(config) {}

        AIResult process(const AIInput &input) const
        {
            return _orchestrator.route(input);
        }

    private:
        AIOrchestrator _orchestrator;
    };

} // namespace HybridAI

// Callback function type nhận vector đặc trưng chuẩn hóa từ Edge AI
typedef void (*FeatureVectorCallback)(const EdgeAI::FeatureVector &features);

// --- HybridAIEngine: Cầu nối điều phối giữa Edge AI và Cloud AI ---
class HybridAIEngine
{
public:
    HybridAIEngine() : _featureCallback(nullptr) {}

    // Đăng ký callback nhận vector đặc trưng sau chuẩn hóa
    void onFeatures(FeatureVectorCallback callback)
    {
        _featureCallback = callback;
    }

    // Đưa mẫu cảm biến vào Edge AI và nhận lại vector đặc trưng sau chuẩn hóa
    EdgeAI::FeatureVector process(float sensorSample)
    {
        EdgeAI::FeatureVector features = edge.process(sensorSample);
        if (_featureCallback != nullptr)
        {
            _featureCallback(features);
        }
        return features;
    }

    void setGeminiApiKey(const char *key)
    {
        gemini.setApiKey(key);
    }

    EdgeAI::Engine edge;
    CloudAI::GeminiClient gemini;
    HybridAI::Engine orchestrator;

private:
    FeatureVectorCallback _featureCallback;
};

#endif /* HYBRID_AI_HPP */
