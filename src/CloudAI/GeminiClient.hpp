#ifndef CLOUD_AI_GEMINI_CLIENT_HPP
#define CLOUD_AI_GEMINI_CLIENT_HPP

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <cJSON.h>

namespace CloudAI
{
    class GeminiClient
    {
    public:
        GeminiClient(const char *apiKey = "", const char *model = "gemini-3.5-flash-lite")
            : _model(model), _timeoutMs(15000)
        {
            setApiKey(apiKey);
        }

        void setApiKey(const char *apiKey)
        {
            if (apiKey != nullptr)
            {
                strncpy(_apiKey, apiKey, sizeof(_apiKey) - 1);
                _apiKey[sizeof(_apiKey) - 1] = '\0';
            }
            else
            {
                _apiKey[0] = '\0';
            }
        }

        void setModel(const char *model)
        {
            if (model != nullptr)
                _model = model;
        }

        bool hasApiKey() const { return strlen(_apiKey) > 0; }
        const char *getModel() const { return _model.c_str(); }

        String ask(const String &prompt, const String &systemInstruction = "")
        {
            if (!hasApiKey())
            {
                return "[Error]: Chưa cấu hình GEMINI_API_KEY.";
            }

            if (WiFi.status() != WL_CONNECTED)
            {
                return "[Error]: ESP32 chưa kết nối WiFi.";
            }

            WiFiClientSecure client;
            client.setInsecure(); // Bỏ qua kiểm tra chứng chỉ SSL cho nhẹ RAM
            client.setTimeout(_timeoutMs / 1000);

            HTTPClient http;
            String url = "https://generativelanguage.googleapis.com/v1beta/models/" + _model + ":generateContent?key=" + String(_apiKey);

            if (!http.begin(client, url))
            {
                return "[Error]: Không thể khởi tạo kết nối HTTPS tới Gemini.";
            }

            http.addHeader("Content-Type", "application/json");
            http.setTimeout(_timeoutMs);

            // Tạo Payload JSON gửi Gemini
            cJSON *root = cJSON_CreateObject();
            cJSON *contents = cJSON_CreateArray();
            cJSON *contentItem = cJSON_CreateObject();
            cJSON *parts = cJSON_CreateArray();
            cJSON *partItem = cJSON_CreateObject();

            cJSON_AddStringToObject(partItem, "text", prompt.c_str());
            cJSON_AddItemToArray(parts, partItem);
            cJSON_AddItemToObject(contentItem, "parts", parts);
            cJSON_AddItemToArray(contents, contentItem);
            cJSON_AddItemToObject(root, "contents", contents);

            if (systemInstruction.length() > 0)
            {
                cJSON *sysObj = cJSON_CreateObject();
                cJSON *sysParts = cJSON_CreateArray();
                cJSON *sysPartItem = cJSON_CreateObject();
                cJSON_AddStringToObject(sysPartItem, "text", systemInstruction.c_str());
                cJSON_AddItemToArray(sysParts, sysPartItem);
                cJSON_AddItemToObject(sysObj, "parts", sysParts);
                cJSON_AddItemToObject(root, "systemInstruction", sysObj);
            }

            char *jsonPayload = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);

            int httpCode = http.POST(jsonPayload);
            free(jsonPayload);

            String reply = "";
            if (httpCode == HTTP_CODE_OK || httpCode == 200)
            {
                String responseBody = http.getString();
                cJSON *resRoot = cJSON_Parse(responseBody.c_str());
                if (resRoot != nullptr)
                {
                    cJSON *candidates = cJSON_GetObjectItem(resRoot, "candidates");
                    if (candidates && cJSON_GetArraySize(candidates) > 0)
                    {
                        cJSON *cand0 = cJSON_GetArrayItem(candidates, 0);
                        cJSON *content = cJSON_GetObjectItem(cand0, "content");
                        if (content)
                        {
                            cJSON *partsArr = cJSON_GetObjectItem(content, "parts");
                            if (partsArr && cJSON_GetArraySize(partsArr) > 0)
                            {
                                cJSON *part0 = cJSON_GetArrayItem(partsArr, 0);
                                cJSON *textItem = cJSON_GetObjectItem(part0, "text");
                                if (textItem && textItem->valuestring)
                                {
                                    reply = String(textItem->valuestring);
                                }
                            }
                        }
                    }
                    cJSON_Delete(resRoot);
                }

                if (reply.length() == 0)
                {
                    reply = "[Error]: Không trích xuất được phản hồi văn bản từ Gemini.";
                }
            }
            else if (httpCode == 429)
            {
                reply = "[Error 429]: Quota của model " + _model + " tạm hết hạn mức. Hệ thống sẽ tự động chuyển sang chế độ Edge phản hồi tại chỗ.";
            }
            else
            {
                reply = "[HTTP " + String(httpCode) + " Error]: " + http.getString();
            }

            http.end();
            return reply;
        }

    private:
        char _apiKey[96];
        String _model;
        uint32_t _timeoutMs;
    };
}

#endif /* CLOUD_AI_GEMINI_CLIENT_HPP */
