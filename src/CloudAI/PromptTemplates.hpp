#ifndef CLOUD_AI_PROMPT_TEMPLATES_HPP
#define CLOUD_AI_PROMPT_TEMPLATES_HPP

#include <Arduino.h>

namespace CloudAI
{
    class PromptTemplates
    {
    public:
        // Sinh prompt phân tích Telemetry cảm biến cho LLM (Gemini / OpenAI)
        static String buildTelemetryPrompt(const String &telemetryJson, const String &contextRules = "")
        {
            String prompt = F("You are an expert AI IoT Diagnostic Agent. Analyze the following real-time device telemetry data:\n");
            prompt += F("```json\n");
            prompt += telemetryJson;
            prompt += F("\n```\n");

            if (contextRules.length() > 0)
            {
                prompt += F("Rules & Context:\n");
                prompt += contextRules;
                prompt += F("\n");
            }

            prompt += F("Respond strictly with a valid JSON object without markdown fences, containing:\n"
                        "{\n"
                        "  \"status\": \"NORMAL\" | \"WARNING\" | \"CRITICAL\",\n"
                        "  \"analysis\": \"<concise explanation>\",\n"
                        "  \"actions\": {\n"
                        "    \"<actuator_or_target>\": <recommended_state_or_value>\n"
                        "  }\n"
                        "}\n");
            return prompt;
        }

        // Sinh prompt cho thị giác máy tính Gemini Vision (ESP32-CAM)
        static String buildVisionPrompt(const String &customTask = "Detect objects, reading meters, and report anomalies.")
        {
            String prompt = F("Analyze this camera frame from an embedded AIoT device. Task: ");
            prompt += customTask;
            prompt += F("\nProvide concise diagnostic output in JSON format with fields: result, anomaly_detected (bool), confidence (0.0-1.0), and recommended_action.");
            return prompt;
        }
    };
}

#endif /* CLOUD_AI_PROMPT_TEMPLATES_HPP */

