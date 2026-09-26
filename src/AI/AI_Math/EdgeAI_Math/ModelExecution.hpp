#ifndef AI_MATH_MODEL_EXECUTION_HPP
#define AI_MATH_MODEL_EXECUTION_HPP

#include <Arduino.h>
#include <math.h>

namespace AI_Math
{
    // =========================================================================
    // GIAI ĐOẠN 3: THƯ VIỆN THỰC THI MẠNG NƠ-RON ĐA MỤC TIÊU ĐỘNG (DYNAMIC ENGINE)
    // -------------------------------------------------------------------------
    // Thư viện thuần túy toán học: T thích nghi với mọi số lượng đầu vào/đầu ra.
    // Không chứa bất kỳ hằng số cứng nào liên quan đến ứng dụng cụ thể.
    // =========================================================================

    class NeuralEngine
    {
    public:
        /**
         * @brief Thực hiện suy luận mạng nơ-ron đa mục tiêu với cấu hình động hoàn toàn
         *
         * @param flattenInput   Con trỏ mảng vector đặc trưng đã chuẩn hóa phẳng (Độ dài = inputDim)
         * @param inputDim       Tổng số lượng chỉ số đặc trưng đầu vào (Số cột của ma trận W)
         * @param numLabels      Số lượng trạng thái/nhãn cần phân loại độc quyền (Cần chạy Softmax)
         * @param numCmds        Số lượng lệnh/thiết bị cần điều khiển độc lập (Cần chạy Sigmoid)
         * @param W              Ma trận trọng số phẳng (Kích thước: (numLabels + numCmds) * inputDim)
         * @param b              Mảng bias định thiên (Kích thước: numLabels + numCmds)
         * @param outLabelsProb  Mảng xuất kết quả xác suất % của các Nhãn (Người dùng tự khai báo mảng ngoài truyền vào)
         * @param outCmdsProb    Mảng xuất kết quả xác suất % của các CMD (Người dùng tự khai báo mảng ngoài truyền vào)
         * @param outExecutionTime Mức đo thời gian thực thi (us) của chip
         * @return size_t        Vị trí Index của nhãn chiến thắng có độ tự tin cao nhất (Argmax)
         */
        static size_t predict(const float *flattenInput, size_t inputDim,
                              size_t numLabels, size_t numCmds,
                              const float *W, const float *b,
                              float *outLabelsProb, float *outCmdsProb,
                              uint32_t *outExecutionTime)
        {
            uint32_t startTime = micros();
            size_t totalOutputs = numLabels + numCmds;

            if (flattenInput == nullptr || W == nullptr || b == nullptr || totalOutputs == 0)
            {
                if (outExecutionTime != nullptr)
                    *outExecutionTime = 0;
                return 0;
            }

            // Tạo mảng logits thô động tạm thời trên Stack (Sử dụng VLA - Variable Length Array hỗ trợ bởi GCC C++)
            float logits[totalOutputs];

            // 1. PHÉP NHÂN MA TRẬN PHẲNG ĐỘNG (Feed Forward)
            for (size_t out = 0; out < totalOutputs; out++)
            {
                float sum = b[out]; // Lấy bias nền của hàng đang tính
                size_t rowOffset = out * inputDim;

                for (size_t in = 0; in < inputDim; in++)
                {
                    sum += flattenInput[in] * W[rowOffset + in];
                }
                logits[out] = sum;
            }

            // 2. THỰC THI TOÁN HỌC NHÓM SOFTMAX (Phân loại nhãn độc quyền)
            size_t predictedIndex = 0;
            if (numLabels > 0)
            {
                // Tìm Max logit của nhóm nhãn để ổn định số mũ (Tránh tràn bộ nhớ số thực)
                float maxLogit = logits[0];
                for (size_t i = 1; i < numLabels; i++)
                {
                    if (logits[i] > maxLogit)
                        maxLogit = logits[i];
                }

                // Tính tổng cơ số mũ e
                float sumExp = 0.0f;
                float expScores[numLabels];
                for (size_t i = 0; i < numLabels; i++)
                {
                    expScores[i] = expf(logits[i] - maxLogit);
                    sumExp += expScores[i];
                }

                // Tính xác suất % và tìm Argmax hằng số chiến thắng
                float maxProb = -1.0f;
                for (size_t i = 0; i < numLabels; i++)
                {
                    float prob = (sumExp > 1e-6f) ? (expScores[i] / sumExp) : 0.0f;
                    if (outLabelsProb != nullptr)
                    {
                        outLabelsProb[i] = prob;
                    }
                    if (prob > maxProb)
                    {
                        maxProb = prob;
                        predictedIndex = i;
                    }
                }
            }

            // 3. THỰC THI TOÁN HỌC NHÓM SIGMOID (Kích hoạt CMD độc lập)
            if (numCmds > 0 && outCmdsProb != nullptr)
            {
                for (size_t i = 0; i < numCmds; i++)
                {
                    size_t cmdLogitIndex = numLabels + i; // Trượt con trỏ đến phân vùng CMD
                    outCmdsProb[i] = 1.0f / (1.0f + expf(-logits[cmdLogitIndex]));
                }
            }

            if (outExecutionTime != nullptr)
            {
                *outExecutionTime = micros() - startTime;
            }

            return predictedIndex; // Trả về kẻ chiến thắng trong nhóm nhãn
        }
    };
}

#endif // AI_MATH_MODEL_EXECUTION_HPP