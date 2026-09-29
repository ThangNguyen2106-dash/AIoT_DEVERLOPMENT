#ifndef HYBRID_AI_H_
#define HYBRID_AI_H_

#include <Arduino.h>
#include <AI/EdgeAI/EdgeAI.hpp>
#include <AI/CloudAI/CloudAI.h>
#include <cJSON.h>
#include <IoT/DEBUG.hpp>

namespace HybridAI
{
    /**
     * @brief Lớp cầu nối dữ liệu và đồng bộ mô hình 2 chiều giữa Edge AI và Cloud AI
     *        Không ép buộc chính sách (Policy-free), cung cấp các công cụ:
     *        1. Đóng gói telemetry 16 đặc trưng dạng JSON chuẩn để gửi MQTT/Cloud
     *        2. Phân tích chuỗi JSON nhận từ Cloud chứa W, b và nạp vào Flash NVS
     *        3. Gọi Gemini phân tích trực tiếp 16 đặc trưng thời gian thực
     */
    class Bridge
    {
    private:
        EdgeAI *_edgeAI;
        CloudAI::GeminiClient *_cloudAI;
        // Buffer nội bộ để tránh stack overflow khi sync model từ Cloud (1720 bytes trên stack là quá lớn)
        float _syncTempW[EdgeAI::MAX_WEIGHTS];      // 384 * 4 = 1536 bytes
        float _syncTempB[EdgeAI::MAX_OUTPUTS];      // 24  * 4 = 96 bytes
        float _syncTempNorm1[EdgeAI::MAX_FEATURES]; // 16  * 4 = 64 bytes
        float _syncTempNorm2[EdgeAI::MAX_FEATURES]; // 16  * 4 = 64 bytes

    public:
        Bridge(EdgeAI *edge = nullptr, CloudAI::GeminiClient *cloud = nullptr)
            : _edgeAI(edge), _cloudAI(cloud)
        {
            memset(_syncTempW,    0, sizeof(_syncTempW));
            memset(_syncTempB,    0, sizeof(_syncTempB));
            memset(_syncTempNorm1, 0, sizeof(_syncTempNorm1));
            memset(_syncTempNorm2, 0, sizeof(_syncTempNorm2));
        }

        void bind(EdgeAI *edge, CloudAI::GeminiClient *cloud = nullptr)
        {
            _edgeAI = edge;
            if (cloud != nullptr)
            {
                _cloudAI = cloud;
            }
        }

        String serializeTelemetry(const char *macAddress = "")
        {
            if (_edgeAI == nullptr)
                return "{}";

            cJSON *root = cJSON_CreateObject();
            if (macAddress && strlen(macAddress) > 0)
            {
                cJSON_AddStringToObject(root, "mac", macAddress);
            }

            cJSON_AddNumberToObject(root, "winner_label", (int)_edgeAI->getWinnerLabel());
            cJSON_AddNumberToObject(root, "confidence", _edgeAI->getWinnerConfidence());
            cJSON_AddStringToObject(root, "norm_type", _edgeAI->getNormTypeName());
            cJSON_AddBoolToObject(root, "from_nvs", _edgeAI->isLoadedFromNVS());
            cJSON_AddNumberToObject(root, "latency_us", (int)_edgeAI->getExecutionTime());

            cJSON *cmds = cJSON_CreateArray();
            for (size_t i = 0; i < 4; i++)
            {
                cJSON_AddItemToArray(cmds, cJSON_CreateNumber(_edgeAI->getCmdScore(i)));
            }
            cJSON_AddItemToObject(root, "cmd_scores", cmds);

            const float *features = _edgeAI->getRawFeatures();
            size_t featCount = _edgeAI->getFeatureCount();
            cJSON *featArr = cJSON_CreateArray();
            for (size_t i = 0; i < featCount; i++)
            {
                cJSON_AddItemToArray(featArr, cJSON_CreateNumber(features[i]));
            }
            cJSON_AddItemToObject(root, "features", featArr);

            char *jsonStr = cJSON_PrintUnformatted(root);
            String result(jsonStr);
            cJSON_Delete(root);
            free(jsonStr);

            return result;
        }

        bool syncModelFromJson(const char *jsonPayload)
        {
            if (_edgeAI == nullptr || jsonPayload == nullptr || strlen(jsonPayload) == 0)
            {
                LOG_ERROR("HYBRID_AI", "syncModelFromJson: EdgeAI instance or payload is null");
                return false;
            }

            cJSON *root = cJSON_Parse(jsonPayload);
            if (root == nullptr)
            {
                LOG_ERROR("HYBRID_AI", "syncModelFromJson: JSON parse error!");
                return false;
            }

            cJSON *wItem = cJSON_GetObjectItem(root, "W");
            cJSON *bItem = cJSON_GetObjectItem(root, "b");
            cJSON *inDimItem = cJSON_GetObjectItem(root, "input_dim");
            cJSON *nLabelsItem = cJSON_GetObjectItem(root, "num_labels");
            cJSON *nCmdsItem = cJSON_GetObjectItem(root, "num_cmds");
            cJSON *normTypeItem = cJSON_GetObjectItem(root, "norm_type");
            cJSON *norm1Item = cJSON_GetObjectItem(root, "norm1");
            cJSON *norm2Item = cJSON_GetObjectItem(root, "norm2");

            if (!wItem || !bItem || !inDimItem || !nLabelsItem)
            {
                LOG_ERROR("HYBRID_AI", "syncModelFromJson: Missing required fields (W, b, input_dim, num_labels)");
                cJSON_Delete(root);
                return false;
            }

            size_t inputDim = inDimItem->valueint;
            size_t numLabels = nLabelsItem->valueint;
            size_t numCmds = nCmdsItem ? nCmdsItem->valueint : 0;
            size_t normTypeVal = normTypeItem ? normTypeItem->valueint : 0;
            EdgeAI::NormType normType = static_cast<EdgeAI::NormType>(normTypeVal);

            size_t totalOutputs = numLabels + numCmds;
            size_t totalWeights = totalOutputs * inputDim;

            if (inputDim > EdgeAI::MAX_FEATURES || totalOutputs > EdgeAI::MAX_OUTPUTS || totalWeights > EdgeAI::MAX_WEIGHTS)
            {
                LOG_ERROR("HYBRID_AI", "syncModelFromJson: Model dimensions exceed max capacity! (InDim:%u, Out:%u, W:%u)",
                          (unsigned)inputDim, (unsigned)totalOutputs, (unsigned)totalWeights);
                cJSON_Delete(root);
                return false;
            }

            int wSize = cJSON_GetArraySize(wItem);
            int bSize = cJSON_GetArraySize(bItem);

            if (wSize != (int)totalWeights || bSize != (int)totalOutputs)
            {
                LOG_ERROR("HYBRID_AI", "syncModelFromJson: Size mismatch! Expected W:%u (got %d), b:%u (got %d)",
                          (unsigned)totalWeights, wSize, (unsigned)totalOutputs, bSize);
                cJSON_Delete(root);
                return false;
            }

            float *tempW = _syncTempW;
            float *tempB = _syncTempB;
            for (size_t i = 0; i < totalWeights; i++)
            {
                cJSON *item = cJSON_GetArrayItem(wItem, i);
                if (item == nullptr)
                {
                    LOG_ERROR("HYBRID_AI", "syncModelFromJson: Null item in W array at index %u", (unsigned)i);
                    cJSON_Delete(root);
                    return false;
                }
                tempW[i] = (float)item->valuedouble;
            }
            for (size_t i = 0; i < totalOutputs; i++)
            {
                cJSON *item = cJSON_GetArrayItem(bItem, i);
                if (item == nullptr)
                {
                    LOG_ERROR("HYBRID_AI", "syncModelFromJson: Null item in b array at index %u", (unsigned)i);
                    cJSON_Delete(root);
                    return false;
                }
                tempB[i] = (float)item->valuedouble;
            }

            float *tempNorm1 = _syncTempNorm1;
            float *tempNorm2 = _syncTempNorm2;
            float *pNorm1 = nullptr;
            float *pNorm2 = nullptr;

            if (normType != EdgeAI::NORM_NONE && norm1Item && norm2Item)
            {
                int n1Size = cJSON_GetArraySize(norm1Item);
                int n2Size = cJSON_GetArraySize(norm2Item);
                if (n1Size == (int)inputDim && n2Size == (int)inputDim)
                {
                    bool normOk = true;
                    for (size_t i = 0; i < inputDim; i++)
                    {
                        cJSON *it1 = cJSON_GetArrayItem(norm1Item, i);
                        cJSON *it2 = cJSON_GetArrayItem(norm2Item, i);
                        if (!it1 || !it2)
                        {
                            normOk = false;
                            break;
                        }
                        tempNorm1[i] = (float)it1->valuedouble;
                        tempNorm2[i] = (float)it2->valuedouble;
                    }
                    if (normOk)
                    {
                        pNorm1 = _syncTempNorm1;
                        pNorm2 = _syncTempNorm2;
                    }
                }
            }

            cJSON_Delete(root);

            bool saved = _edgeAI->saveToNVS(tempW, tempB, inputDim, numLabels, numCmds, normType, pNorm1, pNorm2);
            if (saved)
            {
                LOG_INFO("HYBRID_AI", "Successfully synced and hot-reloaded model from Cloud into NVS Flash!");
            }
            return saved;
        }

        String consultCloud(const String &userQuestion = "")
        {
            if (_edgeAI == nullptr || _cloudAI == nullptr)
            {
                return F("[Error]: EdgeAI hoặc CloudAI chưa được liên kết với HybridAI::Bridge!");
            }

            const float *f = _edgeAI->getRawFeatures();
            size_t count = _edgeAI->getFeatureCount();

            String prompt = F("Dưới đây là 16 chỉ số đặc trưng thực tế trích xuất từ các kênh cảm biến của thiết bị:\n");
            for (size_t c = 0; c < count / 4; c++)
            {
                size_t o = c * 4;
                prompt += "Kênh " + String(c) + ": Mean=" + String(f[o], 2) +
                          ", RMS=" + String(f[o + 1], 2) +
                          ", P2P=" + String(f[o + 2], 2) +
                          ", StdDev=" + String(f[o + 3], 2) + "\n";
            }
            prompt += "Trạng thái Edge AI: Nhãn " + String((int)_edgeAI->getWinnerLabel()) +
                      " (Độ tự tin: " + String(_edgeAI->getWinnerConfidence() * 100.0f, 1) + "%)\n";

            if (userQuestion.length() > 0)
            {
                prompt += "\nCâu hỏi của kỹ sư: " + userQuestion + "\n";
            }
            else
            {
                prompt += F("\nHãy đánh giá sức khỏe thiết bị và đưa ra khuyến nghị kỹ thuật ngắn gọn.\n");
            }

            String sysInstruction = F("Bạn là chuyên gia chẩn đoán tình trạng máy móc công nghiệp AIoT. Hãy phân tích các chỉ số vật lý trên và trả lời bằng tiếng Việt ngắn gọn, súc tích.");

            return _cloudAI->ask(prompt, sysInstruction);
        }
    };
}

#endif /* HYBRID_AI_H_ */
