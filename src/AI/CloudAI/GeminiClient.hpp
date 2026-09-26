#ifndef CLOUD_AI_GEMINI_CLIENT_HPP_
#define CLOUD_AI_GEMINI_CLIENT_HPP_

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <cJSON.h>
#include <lwip/dns.h>
#include <IoT/DEBUG.hpp>

namespace CloudAI
{
    /**
     * @brief Module giao tiếp trực tiếp với Google Gemini REST API qua HTTPS
     *        Hỗ trợ các model như gemini-1.5-flash, gemini-2.0-flash, gemini-3.5-flash-lite
     */
    class GeminiClient
    {
    private:
        char _apiKey[96];
        String _model;
        uint32_t _timeoutMs;

    public:
        GeminiClient(const char *apiKey = "", const char *model = "gemini-1.5-flash")
            : _model(model), _timeoutMs(20000)
        {
            setApiKey(apiKey);
        }

        void begin(const char *apiKey, const char *model = "gemini-1.5-flash")
        {
            setApiKey(apiKey);
            if (model && strlen(model) > 0)
            {
                _model = model;
            }
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
            if (model != nullptr && strlen(model) > 0)
            {
                _model = model;
            }
        }

        void setTimeout(uint32_t timeoutMs)
        {
            _timeoutMs = timeoutMs;
        }

        bool hasApiKey() const { return strlen(_apiKey) > 0; }
        const char *getModel() const { return _model.c_str(); }

        String ask(const String &prompt, const String &systemInstruction = "")
        {
            if (!hasApiKey())
            {
                LOG_WARN("GEMINI", "API Key not configured!");
                return F("[Error]: Chưa cấu hình GEMINI_API_KEY.");
            }

            if (WiFi.status() != WL_CONNECTED)
            {
                LOG_WARN("GEMINI", "WiFi not connected!");
                return F("[Error]: ESP32 chưa kết nối WiFi.");
            }

            if (WiFi.dnsIP(0) == IPAddress(0, 0, 0, 0))
            {
                ip_addr_t d1, d2;
                d1.type = IPADDR_TYPE_V4;
                d1.u_addr.ip4.addr = static_cast<uint32_t>(IPAddress(8, 8, 8, 8));
                dns_setserver(0, &d1);
                d2.type = IPADDR_TYPE_V4;
                d2.u_addr.ip4.addr = static_cast<uint32_t>(IPAddress(1, 1, 1, 1));
                dns_setserver(1, &d2);
            }

            WiFiClientSecure client;
            client.setInsecure();
            client.setTimeout(_timeoutMs / 1000);
            client.setHandshakeTimeout(15);

            HTTPClient http;
            String url = "https://generativelanguage.googleapis.com/v1beta/models/" + _model + ":generateContent?key=" + String(_apiKey);

            if (!http.begin(client, url))
            {
                client.stop();
                return F("[Error]: Không thể khởi tạo kết nối HTTPS tới Gemini.");
            }

            http.addHeader("Content-Type", "application/json");
            http.setTimeout(_timeoutMs);

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
                    reply = F("[Error]: Không trích xuất được phản hồi văn bản từ Gemini.");
                }
            }
            else if (httpCode == 429)
            {
                reply = "[Error 429]: Quota của model " + _model + " tạm hết hạn mức (Too Many Requests).";
            }
            else
            {
                if (httpCode < 0)
                {
                    reply = "[HTTP " + String(httpCode) + " Error]: " + http.errorToString(httpCode);
                }
                else
                {
                    reply = "[HTTP " + String(httpCode) + " Error]: " + http.getString();
                }
            }

            http.end();
            client.stop();
            return reply;
        }
    };
}

#endif /* CLOUD_AI_GEMINI_CLIENT_HPP_ */
