#ifndef AI_MATH_MODEL_EXCUTION_HPP
#define AI_MATH_MODEL_EXCUTION_HPP

#include <Arduino.h>
#include <math.h>

namespace AI_Math
{
    // =====================================================================
    // GIAI ĐOẠN 3: XỬ LÝ TÍNH TOÁN NƠ-RON THÔNG QUA CÁC TRỌNG SỐ (MODEL EXECUTION)
    // =====================================================================

    class Activation
    {
    public:
        static inline float relu(float x)
        {
            return (x > 0.0f) ? x : 0.0f;
        }

        static inline float leakyRelu(float x, float alpha = 0.01f)
        {
            return (x > 0.0f) ? x : (alpha * x);
        }

        static inline float sigmoid(float x)
        {
            if (x > 45.0f)
                return 1.0f;
            if (x < -45.0f)
                return 0.0f;
            return 1.0f / (1.0f + expf(-x));
        }

        static inline float tanhAct(float x)
        {
            return tanhf(x);
        }

        static void softmax(float *data, size_t length)
        {
            if (data == nullptr || length == 0)
                return;
            float maxVal = data[0];
            for (size_t i = 1; i < length; i++)
            {
                if (data[i] > maxVal)
                    maxVal = data[i];
            }
            float sumExp = 0.0f;
            for (size_t i = 0; i < length; i++)
            {
                data[i] = expf(data[i] - maxVal);
                sumExp += data[i];
            }
            if (sumExp > 1e-6f)
            {
                for (size_t i = 0; i < length; i++)
                {
                    data[i] /= sumExp;
                }
            }
        }

        static size_t argmax(const float *data, size_t length)
        {
            if (data == nullptr || length == 0)
                return 0;
            size_t maxIdx = 0;
            float maxVal = data[0];
            for (size_t i = 1; i < length; i++)
            {
                if (data[i] > maxVal)
                {
                    maxVal = data[i];
                    maxIdx = i;
                }
            }
            return maxIdx;
        }
    };

    class NeuralMath
    {
    public:
        static float dotProduct(const float *a, const float *b, size_t length)
        {
            if (a == nullptr || b == nullptr || length == 0)
                return 0.0f;
            float sum = 0.0f;
            for (size_t i = 0; i < length; i++)
            {
                sum += a[i] * b[i];
            }
            return sum;
        }

        static float euclideanDistance(const float *a, const float *b, size_t length)
        {
            if (a == nullptr || b == nullptr || length == 0)
                return 0.0f;
            float sumSq = 0.0f;
            for (size_t i = 0; i < length; i++)
            {
                float diff = a[i] - b[i];
                sumSq += diff * diff;
            }
            return sqrtf(sumSq);
        }

        static void denseForward(const float *W, const float *x, const float *b,
                                 float *y, size_t outDim, size_t inDim)
        {
            if (W == nullptr || x == nullptr || y == nullptr)
                return;
            for (size_t r = 0; r < outDim; r++)
            {
                float sum = (b != nullptr) ? b[r] : 0.0f;
                const float *wRow = &W[r * inDim];
                for (size_t c = 0; c < inDim; c++)
                {
                    sum += wRow[c] * x[c];
                }
                y[r] = sum;
            }
        }
    };

    struct InferenceResult
    {
        static constexpr size_t MAX_CLASSES = 8;
        size_t predictedClass;
        float confidence;
        float probabilities[MAX_CLASSES];
        float logits[MAX_CLASSES];
        size_t numClasses;
        uint32_t executionTimeUs;

        InferenceResult() : predictedClass(0), confidence(0.0f), numClasses(0), executionTimeUs(0)
        {
            for (size_t i = 0; i < MAX_CLASSES; i++)
            {
                probabilities[i] = 0.0f;
                logits[i] = 0.0f;
            }
        }
    };

    class NeuralClassifier
    {
    public:
        // Chuyển thành hàm static hoàn toàn để trùng khớp với mã gọi ở main
        static InferenceResult predict(const float *inputVector, const float *W, const float *b,
                                       size_t outDim, size_t inDim, bool useSoftmax = true)
        {
            InferenceResult result;
            result.numClasses = (outDim > InferenceResult::MAX_CLASSES) ? InferenceResult::MAX_CLASSES : outDim;

            uint32_t startTime = micros();

            // Tính điểm số thô tuyến tính (Logits)
            NeuralMath::denseForward(W, inputVector, b, result.logits, result.numClasses, inDim);

            if (useSoftmax)
            {
                for (size_t i = 0; i < result.numClasses; i++)
                {
                    result.probabilities[i] = result.logits[i];
                }
                Activation::softmax(result.probabilities, result.numClasses);
                result.predictedClass = Activation::argmax(result.probabilities, result.numClasses);
                result.confidence = result.probabilities[result.predictedClass];
            }
            else
            {
                // Sử dụng Sigmoid hàng loạt (Đa nhãn / Đa chỉ số) theo lựa chọn tối ưu của bạn
                for (size_t i = 0; i < result.numClasses; i++)
                {
                    result.probabilities[i] = Activation::sigmoid(result.logits[i]);
                }
                result.predictedClass = Activation::argmax(result.probabilities, result.numClasses);
                result.confidence = result.probabilities[result.predictedClass];
            }

            result.executionTimeUs = micros() - startTime;
            return result;
        }
    };

    class KMeansAnomalyDetector
    {
    public:
        // Sửa hàm static nhận con trỏ xuất điểm anomalyScore, khớp với logic loop() của bạn
        static bool isAnomaly(const float *inputVector, const float *clusterCenters,
                              size_t numClusters, size_t length, float threshold, float *outAnomalyScore = nullptr)
        {
            if (inputVector == nullptr || clusterCenters == nullptr || numClusters == 0)
                return false;

            float minDistance = 1e9f;
            for (size_t i = 0; i < numClusters; i++)
            {
                const float *center = &clusterCenters[i * length];
                float dist = NeuralMath::euclideanDistance(inputVector, center, length);
                if (dist < minDistance)
                    minDistance = dist;
            }

            if (outAnomalyScore != nullptr)
            {
                *outAnomalyScore = minDistance; // Trả về khoảng cách thô làm Score đánh giá bất thường
            }

            return (minDistance > threshold);
        }
    };
}
#endif