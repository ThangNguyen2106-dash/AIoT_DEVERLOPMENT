#ifndef AI_MATH_ONLINE_LEARNING_HPP
#define AI_MATH_ONLINE_LEARNING_HPP

#include <Arduino.h>
#include <math.h>
#include "Matrix.hpp"

namespace AI_Math
{
    // Thuật toán Welford: Cập nhật Mean và Variance tăng dần với độ phức tạp O(1) bộ nhớ
    class WelfordEstimator
    {
    public:
        WelfordEstimator() : count(0), mean(0.0f), M2(0.0f) {}

        void update(float x)
        {
            count++;
            float delta = x - mean;
            mean += delta / (float)count;
            float delta2 = x - mean;
            M2 += delta * delta2;
        }

        void reset()
        {
            count = 0;
            mean = 0.0f;
            M2 = 0.0f;
        }

        // Khởi tạo điểm chuẩn danh định (Nominal Baseline & Tolerance) trực tiếp
        void seed(float initialMean, float initialStdDev, uint32_t virtualCount = 30)
        {
            count = (virtualCount < 2) ? 2 : virtualCount;
            mean = initialMean;
            M2 = (initialStdDev * initialStdDev) * (float)(count - 1);
        }

        uint32_t getCount() const { return count; }
        float getMean() const { return mean; }
        float getVariance() const { return (count > 1) ? (M2 / (float)(count - 1)) : 0.0f; }
        float getStdDev() const { return sqrtf(getVariance()); }

        // Tính điểm bất thường Z-Score của một mẫu mới
        float computeZScore(float x, float minStdDev = 1.2f) const
        {
            float s = getStdDev();
            // Đảm bảo luôn có sàn độ biến thiên tối thiểu (Noise Floor) để tránh chia cho 0 
            // và đảm bảo phát hiện được sự cố ngay cả khi môi trường lúc học hoàn toàn đứng yên (StdDev = 0)
            // và lọc nhiễu tự nhiên của cảm biến (ví dụ DHT11 có bước nhảy tối thiểu 1.0°C)
            if (s < minStdDev)
                s = minStdDev;
            return fabsf(x - mean) / s;
        }

    private:
        uint32_t count;
        float mean;
        float M2;
    };

    // Bộ phân loại k-Nearest Neighbors (k-NN) chạy và học trực tiếp trên vi điều khiển
    template <size_t FEATURE_DIM, size_t MAX_SAMPLES, size_t K_NEIGHBORS = 3>
    class OnlineKNN
    {
    public:
        OnlineKNN() : sampleCount(0) {}

        // Thêm một mẫu huấn luyện vào bộ nhớ
        bool addSample(const float *features, int label)
        {
            if (sampleCount >= MAX_SAMPLES || features == nullptr)
                return false;

            for (size_t d = 0; d < FEATURE_DIM; d++)
            {
                dataset[sampleCount][d] = features[d];
            }
            labels[sampleCount] = label;
            sampleCount++;
            return true;
        }

        void clear()
        {
            sampleCount = 0;
        }

        size_t count() const { return sampleCount; }

        // Dự đoán nhãn cho vector đặc trưng mới
        int predict(const float *queryFeatures, float *minDistOut = nullptr) const
        {
            if (sampleCount == 0 || queryFeatures == nullptr)
                return -1;

            float distances[MAX_SAMPLES];
            size_t indices[MAX_SAMPLES];

            for (size_t i = 0; i < sampleCount; i++)
            {
                distances[i] = Matrix::euclideanDistance(queryFeatures, dataset[i], FEATURE_DIM);
                indices[i] = i;
            }

            // Sắp xếp tìm K láng giềng gần nhất (Selection Sort tối ưu cho mảng nhỏ)
            size_t k = (K_NEIGHBORS > sampleCount) ? sampleCount : K_NEIGHBORS;
            for (size_t i = 0; i < k; i++)
            {
                size_t minIdx = i;
                for (size_t j = i + 1; j < sampleCount; j++)
                {
                    if (distances[j] < distances[minIdx])
                    {
                        minIdx = j;
                    }
                }
                // Hoán đổi
                float tempD = distances[i];
                distances[i] = distances[minIdx];
                distances[minIdx] = tempD;

                size_t tempIdx = indices[i];
                indices[i] = indices[minIdx];
                indices[minIdx] = tempIdx;
            }

            if (minDistOut != nullptr)
            {
                *minDistOut = distances[0];
            }

            // Bầu chọn nhãn chiếm đa số trong K láng giềng
            int voteLabel = labels[indices[0]];
            return voteLabel;
        }

    private:
        float dataset[MAX_SAMPLES][FEATURE_DIM];
        int labels[MAX_SAMPLES];
        size_t sampleCount;
    };

    // Bộ phân cụm K-Means Online thích ứng
    template <size_t FEATURE_DIM, size_t K_CLUSTERS>
    class OnlineKMeans
    {
    public:
        OnlineKMeans(float learningRate = 0.05f) : _lr(learningRate), _trained(false)
        {
            for (size_t k = 0; k < K_CLUSTERS; k++)
            {
                counts[k] = 0;
                for (size_t d = 0; d < FEATURE_DIM; d++)
                {
                    centroids[k][d] = 0.0f;
                }
            }
        }

        // Khởi tạo tâm cụm ban đầu
        void initCentroid(size_t clusterIdx, const float *features)
        {
            if (clusterIdx >= K_CLUSTERS || features == nullptr)
                return;
            for (size_t d = 0; d < FEATURE_DIM; d++)
            {
                centroids[clusterIdx][d] = features[d];
            }
            _trained = true;
        }

        // Cập nhật tâm cụm tăng dần khi có dữ liệu mới (Online Clustering)
        size_t update(const float *features)
        {
            if (features == nullptr)
                return 0;

            size_t bestCluster = predict(features);
            counts[bestCluster]++;

            // Di chuyển centroid về phía điểm dữ liệu mới theo tốc độ học _lr
            for (size_t d = 0; d < FEATURE_DIM; d++)
            {
                centroids[bestCluster][d] += _lr * (features[d] - centroids[bestCluster][d]);
            }
            return bestCluster;
        }

        // Tìm cụm gần nhất
        size_t predict(const float *features, float *distOut = nullptr) const
        {
            if (features == nullptr)
                return 0;

            size_t bestK = 0;
            float minDist = Matrix::euclideanDistance(features, centroids[0], FEATURE_DIM);

            for (size_t k = 1; k < K_CLUSTERS; k++)
            {
                float d = Matrix::euclideanDistance(features, centroids[k], FEATURE_DIM);
                if (d < minDist)
                {
                    minDist = d;
                    bestK = k;
                }
            }

            if (distOut != nullptr)
            {
                *distOut = minDist;
            }
            return bestK;
        }

    private:
        float centroids[K_CLUSTERS][FEATURE_DIM];
        uint32_t counts[K_CLUSTERS];
        float _lr;
        bool _trained;
    };
}

#endif /* AI_MATH_ONLINE_LEARNING_HPP */

