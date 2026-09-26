#ifndef EDGE_AI_HPP_
#define EDGE_AI_HPP_

#include <Arduino.h>
#include <Preferences.h>
#include <AI/AI_Math/EdgeAI_Math/EdgeAI_Math.h>
#include <IoT/DEBUG.hpp>

#ifndef LOG_AI
#define LOG_AI(tag, format, ...) LOG_INFO(tag, format, ##__VA_ARGS__)
#endif

/**
 * =============================================================================
 *                       AIoT EDGE AI PROTOCOL & PIPELINE
 * =============================================================================
 * Quản lý và liên kết toàn bộ quy trình Edge AI từ dữ liệu cảm biến thô đến
 * kết quả phân loại và điều khiển:
 *   Giai đoạn 1: Tiền xử lý (Kalman Filter + Circular Buffer / Sliding Window)
 *   Giai đoạn 2: Trích xuất đặc trưng (Mean, RMS, P2P, StdDev) & Chuẩn hóa (MinMax/ZScore)
 *   Giai đoạn 3: Thực thi nơ-ron đa mục tiêu (Softmax Classify + Sigmoid Commands)
 *   Giai đoạn 4: Quản lý mô hình thích nghi NVS Flash (Lưu trữ / Hot-reload)
 * =============================================================================
 */

class EdgeAI
{
public:
    static const size_t MAX_CHANNELS = 4;                         // Hỗ trợ tối đa 4 kênh cảm biến đồng thời
    static const size_t MAX_WINDOW = 32;                          // Kích thước tối đa của cửa sổ trượt
    static const size_t MAX_FEATURES = 16;                        // Tối đa 4 kênh * 4 đặc trưng = 16
    static const size_t MAX_LABELS = 16;                          // Tối đa 16 nhãn phân loại (Softmax)
    static const size_t MAX_CMDS = 8;                             // Tối đa 8 lệnh điều khiển độc lập (Sigmoid)
    static const size_t MAX_OUTPUTS = MAX_LABELS + MAX_CMDS;      // Tối đa 24 đầu ra
    static const size_t MAX_WEIGHTS = MAX_OUTPUTS * MAX_FEATURES; // Tối đa 384 trọng số

    enum NormType
    {
        NORM_NONE = 0,
        NORM_MIN_MAX = 1,
        NORM_Z_SCORE = 2
    };

    struct ModelHeaderNVS
    {
        uint32_t magic;        // 0x41496F54 = 'AIoT'
        uint16_t version;      // 1
        uint8_t normType;      // NormType (0: None, 1: MinMax, 2: ZScore)
        uint8_t reserved;      // Alignment padding
        uint16_t inputDim;     // Số chiều đặc trưng đầu vào
        uint16_t numLabels;    // Số nhãn Softmax
        uint16_t numCmds;      // Số lệnh Sigmoid
        uint16_t totalOutputs; // numLabels + numCmds
        uint32_t totalWeights; // totalOutputs * inputDim
        uint32_t crc;          // Checksum kiểm tra toàn vẹn
    };

private:
    // Cấu hình Pipeline
    size_t _numChannels;
    size_t _windowSize;
    NormType _normType;

    // Giai đoạn 1: Lọc Kalman & Đệm vòng cho từng kênh
    AI_Math::KalmanFilter _filters[MAX_CHANNELS];
    AI_Math::CircularBuffer<MAX_WINDOW> _buffers[MAX_CHANNELS];

    // Giai đoạn 2: Mảng lưu đặc trưng & thông số chuẩn hóa
    float _rawFeatures[MAX_FEATURES];
    float _normFeatures[MAX_FEATURES];
    const float *_normParam1; // minVals (MinMax) hoặc meanVals (ZScore)
    const float *_normParam2; // maxVals (MinMax) hoặc stdDevVals (ZScore)

    // Giai đoạn 3: Trọng số mô hình & Bộ nhớ kết quả suy luận
    const float *_W;
    const float *_b;
    size_t _inputDim;
    size_t _numLabels;
    size_t _numCmds;

    float _labelsProb[MAX_LABELS];
    float _cmdsProb[MAX_CMDS];
    size_t _winnerLabel;
    float _winnerProb;
    uint32_t _lastExecTimeUs;

    // Giai đoạn 4: Bộ đệm RAM động cho mô hình nạp từ NVS Flash
    float _dynamicW[MAX_WEIGHTS];
    float _dynamicB[MAX_OUTPUTS];
    float _dynamicNorm1[MAX_FEATURES];
    float _dynamicNorm2[MAX_FEATURES];
    bool _isModelLoadedFromNVS;

public:
    EdgeAI()
        : _numChannels(1),
          _windowSize(16),
          _normType(NORM_NONE),
          _normParam1(nullptr),
          _normParam2(nullptr),
          _W(nullptr),
          _b(nullptr),
          _inputDim(0),
          _numLabels(0),
          _numCmds(0),
          _winnerLabel(0),
          _winnerProb(0.0f),
          _lastExecTimeUs(0),
          _isModelLoadedFromNVS(false)
    {
        for (size_t i = 0; i < MAX_FEATURES; i++)
        {
            _rawFeatures[i] = 0.0f;
            _normFeatures[i] = 0.0f;
            _dynamicNorm1[i] = 0.0f;
            _dynamicNorm2[i] = 0.0f;
        }
        for (size_t i = 0; i < MAX_LABELS; i++)
        {
            _labelsProb[i] = 0.0f;
        }
        for (size_t i = 0; i < MAX_CMDS; i++)
        {
            _cmdsProb[i] = 0.0f;
        }
        for (size_t i = 0; i < MAX_WEIGHTS; i++)
        {
            _dynamicW[i] = 0.0f;
        }
        for (size_t i = 0; i < MAX_OUTPUTS; i++)
        {
            _dynamicB[i] = 0.0f;
        }
    }

    /**
     * @brief Khởi tạo Edge AI Pipeline
     * @param windowSize Số lượng mẫu cửa sổ trượt cần thu thập trước khi suy luận (mặc định: 16)
     * @param numChannels Số kênh cảm biến (mặc định: 1, tối đa MAX_CHANNELS)
     */
    void begin(size_t windowSize = 16, size_t numChannels = 1)
    {
        if (windowSize > MAX_WINDOW)
            windowSize = MAX_WINDOW;
        if (windowSize == 0)
            windowSize = 16;
        if (numChannels > MAX_CHANNELS)
            numChannels = MAX_CHANNELS;
        if (numChannels == 0)
            numChannels = 1;

        _windowSize = windowSize;
        _numChannels = numChannels;
        clear();

        LOG_INFO("EDGE_AI", "Edge AI Pipeline initialized: %u Channel(s), Window Size: %u",
                 (unsigned)_numChannels, (unsigned)_windowSize);
    }

    /**
     * @brief Cấu hình bộ lọc Kalman cho kênh cảm biến
     * @param q Sai số quá trình (Process noise covariance, vd: 0.01)
     * @param r Sai số đo lường (Measurement noise covariance, vd: 0.1)
     * @param channel Kênh cảm biến (mặc định: 0)
     */
    void setFilter(float q, float r, size_t channel = 0)
    {
        if (channel < MAX_CHANNELS)
        {
            _filters[channel].setParameters(q, r);
            LOG_DEBUG("EDGE_AI", "Channel %u filter set: q=%.4f, r=%.4f", (unsigned)channel, q, r);
        }
    }

    /**
     * @brief Nạp trọng số và cấu hình mạng nơ-ron từ mã nguồn (Flash Code Const)
     * @param W Ma trận trọng số phẳng ((numLabels + numCmds) * inputDim)
     * @param b Mảng bias (numLabels + numCmds)
     * @param inputDim Số lượng chỉ số đầu vào (phải khớp numChannels * 4)
     * @param numLabels Số lượng nhãn phân loại (Softmax)
     * @param numCmds Số lượng lệnh điều khiển (Sigmoid, tùy chọn)
     */
    void setModel(const float *W, const float *b, size_t inputDim, size_t numLabels, size_t numCmds = 0)
    {
        _W = W;
        _b = b;
        _inputDim = inputDim;
        _numLabels = (numLabels > MAX_LABELS) ? MAX_LABELS : numLabels;
        _numCmds = (numCmds > MAX_CMDS) ? MAX_CMDS : numCmds;
        _isModelLoadedFromNVS = false;

        LOG_INFO("EDGE_AI", "Model loaded (Code): Inputs=%u, Labels=%u, Commands=%u",
                 (unsigned)_inputDim, (unsigned)_numLabels, (unsigned)_numCmds);
    }

    /**
     * @brief Cài đặt tham số chuẩn hóa Min - Max
     * @param minVals Mảng giá trị nhỏ nhất của các đặc trưng (độ dài >= inputDim)
     * @param maxVals Mảng giá trị lớn nhất của các đặc trưng (độ dài >= inputDim)
     */
    void setNormalization(const float *minVals, const float *maxVals)
    {
        _normParam1 = minVals;
        _normParam2 = maxVals;
        _normType = NORM_MIN_MAX;
        LOG_INFO("EDGE_AI", "Feature normalization: Min-Max configured");
    }

    /**
     * @brief Cài đặt tham số chuẩn hóa Z - Score
     * @param meanVals Mảng giá trị kỳ vọng trung bình
     * @param stdDevVals Mảng độ lệch chuẩn
     */
    void setZScore(const float *meanVals, const float *stdDevVals)
    {
        _normParam1 = meanVals;
        _normParam2 = stdDevVals;
        _normType = NORM_Z_SCORE;
        LOG_INFO("EDGE_AI", "Feature normalization: Z-Score configured");
    }

    // =========================================================================
    // QUẢN LÝ MÔ HÌNH THÍCH NGHI QUA NVS FLASH (ADAPTIVE AI / HOT-RELOAD)
    // =========================================================================

    /**
     * @brief Hàm băm CRC32 nhẹ kiểm tra toàn vẹn mảng số thực
     */
    static uint32_t calculateCRC(const float *data, size_t count)
    {
        if (!data || count == 0)
            return 0;
        const uint8_t *bytes = reinterpret_cast<const uint8_t *>(data);
        size_t byteCount = count * sizeof(float);
        uint32_t crc = 0x55AA55AA;
        for (size_t i = 0; i < byteCount; i++)
        {
            crc = (crc << 5) | (crc >> 27);
            crc ^= bytes[i];
        }
        return crc;
    }

    /**
     * @brief Đọc và nạp mô hình thích nghi đã lưu trong NVS Flash (nếu có)
     * @return true nếu tìm thấy mô hình hợp lệ và đã nạp thành công, false nếu chưa có hoặc lỗi
     */
    bool loadFromNVS()
    {
        Preferences prefs;
        if (!prefs.begin("edge_ai", true))
        {
            LOG_WARN("EDGE_AI", "NVS: Cannot open namespace 'edge_ai'");
            return false;
        }

        if (!prefs.isKey("hdr"))
        {
            LOG_INFO("EDGE_AI", "NVS: No saved model found in Flash");
            prefs.end();
            return false;
        }

        ModelHeaderNVS hdr;
        if (prefs.getBytes("hdr", &hdr, sizeof(hdr)) != sizeof(hdr))
        {
            LOG_WARN("EDGE_AI", "NVS: Failed to read model header");
            prefs.end();
            return false;
        }

        if (hdr.magic != 0x41496F54 || hdr.version != 1)
        {
            LOG_WARN("EDGE_AI", "NVS: Invalid magic (0x%08X) or version (%u)", (unsigned)hdr.magic, (unsigned)hdr.version);
            prefs.end();
            return false;
        }

        size_t totalOutputs = hdr.totalOutputs;
        size_t totalWeights = hdr.totalWeights;
        size_t inputDim = hdr.inputDim;

        if (totalOutputs > MAX_OUTPUTS || totalWeights > MAX_WEIGHTS || inputDim > MAX_FEATURES)
        {
            LOG_ERROR("EDGE_AI", "NVS: Model dimensions exceed max capacity!");
            prefs.end();
            return false;
        }

        size_t wBytes = totalWeights * sizeof(float);
        if (prefs.getBytes("w", _dynamicW, sizeof(_dynamicW)) < wBytes)
        {
            LOG_ERROR("EDGE_AI", "NVS: Failed to read weights W");
            prefs.end();
            return false;
        }

        size_t bBytes = totalOutputs * sizeof(float);
        if (prefs.getBytes("b", _dynamicB, sizeof(_dynamicB)) < bBytes)
        {
            LOG_ERROR("EDGE_AI", "NVS: Failed to read bias b");
            prefs.end();
            return false;
        }

        uint32_t calcCrc = calculateCRC(_dynamicW, totalWeights) ^ calculateCRC(_dynamicB, totalOutputs);

        NormType norm = static_cast<NormType>(hdr.normType);
        if (norm != NORM_NONE && inputDim > 0)
        {
            size_t nBytes = inputDim * sizeof(float);
            prefs.getBytes("n1", _dynamicNorm1, sizeof(_dynamicNorm1));
            prefs.getBytes("n2", _dynamicNorm2, sizeof(_dynamicNorm2));
            calcCrc ^= calculateCRC(_dynamicNorm1, inputDim) ^ calculateCRC(_dynamicNorm2, inputDim);

            _normParam1 = _dynamicNorm1;
            _normParam2 = _dynamicNorm2;
        }
        else
        {
            _normParam1 = nullptr;
            _normParam2 = nullptr;
            norm = NORM_NONE;
        }

        prefs.end();

        if (calcCrc != hdr.crc)
        {
            LOG_ERROR("EDGE_AI", "NVS: Checksum mismatch! Corrupted data in Flash.");
            return false;
        }

        // Kích hoạt mô hình động vào pipeline
        _inputDim = inputDim;
        _numLabels = hdr.numLabels;
        _numCmds = hdr.numCmds;
        _normType = norm;
        _W = _dynamicW;
        _b = _dynamicB;
        _isModelLoadedFromNVS = true;

        LOG_INFO("EDGE_AI", "NVS: Model successfully loaded from Flash! (Inputs=%u, Labels=%u, Cmds=%u, Norm=%s)",
                 (unsigned)_inputDim, (unsigned)_numLabels, (unsigned)_numCmds, getNormTypeName());
        return true;
    }

    /**
     * @brief Lưu bộ trọng số và kiểu chuẩn hóa mới xuống NVS Flash và kích hoạt áp dụng ngay (Hot-reload)
     */
    bool saveToNVS(const float *W, const float *b, size_t inputDim, size_t numLabels, size_t numCmds,
                   NormType normType = NORM_NONE, const float *norm1 = nullptr, const float *norm2 = nullptr)
    {
        if (W == nullptr || b == nullptr || inputDim == 0)
        {
            LOG_ERROR("EDGE_AI", "saveToNVS: Invalid pointers or inputDim is 0");
            return false;
        }

        size_t totalOutputs = numLabels + numCmds;
        size_t totalWeights = totalOutputs * inputDim;

        if (totalOutputs > MAX_OUTPUTS || totalWeights > MAX_WEIGHTS || inputDim > MAX_FEATURES)
        {
            LOG_ERROR("EDGE_AI", "saveToNVS: Model dimensions exceed max capacity!");
            return false;
        }

        Preferences prefs;
        if (!prefs.begin("edge_ai", false))
        {
            LOG_ERROR("EDGE_AI", "saveToNVS: Cannot open NVS namespace 'edge_ai'");
            return false;
        }

        memcpy(_dynamicW, W, totalWeights * sizeof(float));
        memcpy(_dynamicB, b, totalOutputs * sizeof(float));

        if (normType != NORM_NONE && norm1 != nullptr && norm2 != nullptr)
        {
            memcpy(_dynamicNorm1, norm1, inputDim * sizeof(float));
            memcpy(_dynamicNorm2, norm2, inputDim * sizeof(float));
            _normParam1 = _dynamicNorm1;
            _normParam2 = _dynamicNorm2;
        }
        else
        {
            _normParam1 = nullptr;
            _normParam2 = nullptr;
            normType = NORM_NONE;
        }

        uint32_t crc = calculateCRC(_dynamicW, totalWeights) ^ calculateCRC(_dynamicB, totalOutputs);
        if (normType != NORM_NONE)
        {
            crc ^= calculateCRC(_dynamicNorm1, inputDim) ^ calculateCRC(_dynamicNorm2, inputDim);
        }

        ModelHeaderNVS hdr;
        hdr.magic = 0x41496F54; // 'AIoT'
        hdr.version = 1;
        hdr.normType = static_cast<uint8_t>(normType);
        hdr.reserved = 0;
        hdr.inputDim = static_cast<uint16_t>(inputDim);
        hdr.numLabels = static_cast<uint16_t>(numLabels);
        hdr.numCmds = static_cast<uint16_t>(numCmds);
        hdr.totalOutputs = static_cast<uint16_t>(totalOutputs);
        hdr.totalWeights = static_cast<uint32_t>(totalWeights);
        hdr.crc = crc;

        prefs.putBytes("hdr", &hdr, sizeof(hdr));
        prefs.putBytes("w", _dynamicW, totalWeights * sizeof(float));
        prefs.putBytes("b", _dynamicB, totalOutputs * sizeof(float));
        if (normType != NORM_NONE)
        {
            prefs.putBytes("n1", _dynamicNorm1, inputDim * sizeof(float));
            prefs.putBytes("n2", _dynamicNorm2, inputDim * sizeof(float));
        }
        prefs.end();

        _W = _dynamicW;
        _b = _dynamicB;
        _inputDim = inputDim;
        _numLabels = numLabels;
        _numCmds = numCmds;
        _normType = normType;
        _isModelLoadedFromNVS = true;

        LOG_INFO("EDGE_AI", "saveToNVS: Model successfully saved to NVS and hot-reloaded! (Inputs=%u, Labels=%u, Cmds=%u, Norm=%s)",
                 (unsigned)_inputDim, (unsigned)_numLabels, (unsigned)_numCmds, getNormTypeName());
        return true;
    }

    /**
     * @brief Lưu cấu hình mô hình đang chạy hiện tại xuống NVS Flash
     */
    bool saveCurrentToNVS()
    {
        if (_W == nullptr || _b == nullptr || _inputDim == 0)
        {
            LOG_WARN("EDGE_AI", "saveCurrentToNVS: No active model loaded to save!");
            return false;
        }
        return saveToNVS(_W, _b, _inputDim, _numLabels, _numCmds, _normType, _normParam1, _normParam2);
    }

    /**
     * @brief Xóa trắng mô hình trong NVS Flash (Khôi phục cài đặt gốc - Factory Reset)
     */
    bool clearNVS()
    {
        Preferences prefs;
        if (prefs.begin("edge_ai", false))
        {
            prefs.clear();
            prefs.end();
            _isModelLoadedFromNVS = false;
            LOG_INFO("EDGE_AI", "clearNVS: Model data cleared from Flash (Factory Reset)");
            return true;
        }
        return false;
    }

    bool isLoadedFromNVS() const { return _isModelLoadedFromNVS; }
    NormType getNormType() const { return _normType; }
    const char *getNormTypeName() const
    {
        switch (_normType)
        {
        case NORM_MIN_MAX:
            return "MIN_MAX";
        case NORM_Z_SCORE:
            return "Z_SCORE";
        default:
            return "NONE";
        }
    }

    // =========================================================================
    // DỮ LIỆU CẢM BIẾN (INGESTION)
    // =========================================================================

    /**
     * @brief Đẩy một mẫu dữ liệu thô vào kênh 0 (tự động lọc Kalman)
     * @param rawSample Giá trị thô đọc trực tiếp từ cảm biến
     * @return float Giá trị sạch sau lọc Kalman
     */
    float push(float rawSample)
    {
        return push(0, rawSample);
    }

    /**
     * @brief Đẩy một mẫu dữ liệu thô vào kênh chỉ định (tự động lọc Kalman)
     * @param channel Kênh cảm biến (0 .. numChannels - 1)
     * @param rawSample Giá trị thô
     * @return float Giá trị sạch sau lọc Kalman
     */
    float push(size_t channel, float rawSample)
    {
        if (channel >= _numChannels)
            return rawSample;

        float clean = _filters[channel].update(rawSample);
        _buffers[channel].push(clean);
        return clean;
    }

    /**
     * @brief Đẩy trực tiếp một mẫu đã lọc vào đệm vòng (bỏ qua Kalman)
     */
    void pushClean(size_t channel, float cleanSample)
    {
        if (channel < _numChannels)
        {
            _buffers[channel].push(cleanSample);
        }
    }

    /**
     * @brief Kiểm tra xem bộ đệm đã thu thập đủ số lượng mẫu cửa sổ trượt chưa
     */
    bool isReady() const
    {
        for (size_t c = 0; c < _numChannels; c++)
        {
            if (_buffers[c].size() < _windowSize)
            {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Lấy số lượng mẫu thực tế hiện có trong kênh
     */
    size_t sampleCount(size_t channel = 0) const
    {
        if (channel < _numChannels)
        {
            return _buffers[channel].size();
        }
        return 0;
    }

    // =========================================================================
    // SUY LUẬN AI (INFERENCE EXECUTION)
    // =========================================================================

    /**
     * @brief Chạy toàn bộ Pipeline suy luận AI:
     *        Trích xuất đặc trưng -> Chuẩn hóa vector -> Thực thi NeuralEngine
     * @return size_t Index của nhãn chiến thắng (Winner Class Index)
     */
    size_t predict()
    {
        // 1. Trích xuất đặc trưng cho từng kênh cảm biến
        for (size_t c = 0; c < _numChannels; c++)
        {
            float tempSamples[MAX_WINDOW];
            _buffers[c].toArray(tempSamples);

            size_t featureOffset = c * 4;
            AI_Math::FeatureExtractor::extract(tempSamples, _buffers[c].size(), &_rawFeatures[featureOffset]);
        }

        size_t totalFeatures = _numChannels * 4;
        if (_inputDim > 0 && _inputDim < totalFeatures)
        {
            totalFeatures = _inputDim;
        }

        // 2. Chuẩn hóa vector đặc trưng
        switch (_normType)
        {
        case NORM_MIN_MAX:
            if (_normParam1 && _normParam2)
            {
                AI_Math::FeatureExtractor::normalizeMinMax(
                    _rawFeatures, _normParam1, _normParam2, _normFeatures, totalFeatures);
            }
            break;

        case NORM_Z_SCORE:
            if (_normParam1 && _normParam2)
            {
                AI_Math::FeatureExtractor::normalizeZScore(
                    _rawFeatures, _normParam1, _normParam2, _normFeatures, totalFeatures);
            }
            break;

        default:
            for (size_t i = 0; i < totalFeatures; i++)
            {
                _normFeatures[i] = _rawFeatures[i];
            }
            break;
        }

        // 3. Thực thi suy luận mạng nơ-ron
        if (_W != nullptr && _b != nullptr && (_numLabels > 0 || _numCmds > 0))
        {
            _winnerLabel = AI_Math::NeuralEngine::predict(
                _normFeatures, totalFeatures,
                _numLabels, _numCmds,
                _W, _b,
                _labelsProb, _cmdsProb,
                &_lastExecTimeUs);

            _winnerProb = (_winnerLabel < _numLabels) ? _labelsProb[_winnerLabel] : 0.0f;
        }
        else
        {
            _winnerLabel = 0;
            _winnerProb = 0.0f;
            _lastExecTimeUs = 0;
            LOG_WARN("EDGE_AI", "Predict called but model weights (W, b) not set!");
        }

        return _winnerLabel;
    }

    /**
     * @brief Bí danh tương đương cho predict()
     */
    size_t process()
    {
        return predict();
    }

    // =========================================================================
    // TRUY XUẤT KẾT QUẢ & ĐO ĐẠC (METRICS)
    // =========================================================================

    size_t getWinnerLabel() const { return _winnerLabel; }
    float getWinnerConfidence() const { return _winnerProb; }

    float getConfidence(size_t labelIndex) const
    {
        if (labelIndex < _numLabels)
        {
            return _labelsProb[labelIndex];
        }
        return 0.0f;
    }

    bool getCmdState(size_t cmdIndex, float threshold = 0.5f) const
    {
        if (cmdIndex < _numCmds)
        {
            return _cmdsProb[cmdIndex] >= threshold;
        }
        return false;
    }

    float getCmdScore(size_t cmdIndex) const
    {
        if (cmdIndex < _numCmds)
        {
            return _cmdsProb[cmdIndex];
        }
        return 0.0f;
    }

    uint32_t getExecutionTime() const { return _lastExecTimeUs; }

    const float *getRawFeatures() const { return _rawFeatures; }
    const float *getFeatures() const { return _normFeatures; }
    size_t getFeatureCount() const { return _numChannels * 4; }

    // =========================================================================
    // TIỆN ÍCH GỠ LỖI & RESET
    // =========================================================================

    /**
     * @brief In toàn bộ bảng tóm tắt kết quả suy luận ra Serial (hỗ trợ màu ANSI)
     */
    void printSummary() const
    {
        LOG_AI("EDGE_AI", "=== INFERENCE SUMMARY ===");
        LOG_AI("EDGE_AI", "Source: %s | Norm: %s",
               _isModelLoadedFromNVS ? "NVS Flash (Adaptive)" : "Code (Factory Default)",
               getNormTypeName());
        LOG_AI("EDGE_AI", "Winner Label: %u (Confidence: %.2f%%) | Exec Time: %lu us",
               (unsigned)_winnerLabel, _winnerProb * 100.0f, (unsigned long)_lastExecTimeUs);

        if (_numLabels > 0)
        {
            for (size_t i = 0; i < _numLabels; i++)
            {
                LOG_AI("EDGE_AI", "  [Label %u] Prob: %.2f%%%s",
                       (unsigned)i, _labelsProb[i] * 100.0f, (i == _winnerLabel) ? "  <-- WINNER" : "");
            }
        }
        if (_numCmds > 0)
        {
            for (size_t i = 0; i < _numCmds; i++)
            {
                LOG_AI("EDGE_AI", "  [Cmd %u] Score: %.2f%% (%s)",
                       (unsigned)i, _cmdsProb[i] * 100.0f, (_cmdsProb[i] >= 0.5f) ? "ACTIVE" : "INACTIVE");
            }
        }
    }

    /**
     * @brief In vector đặc trưng của các kênh cảm biến ra Serial
     */
    void printFeatures() const
    {
        for (size_t c = 0; c < _numChannels; c++)
        {
            size_t offset = c * 4;
            LOG_AI("EDGE_AI", "[CH_%u] Mean: %.2f | RMS: %.2f | P2P: %.2f | StdDev: %.2f",
                   (unsigned)c, _rawFeatures[offset], _rawFeatures[offset + 1],
                   _rawFeatures[offset + 2], _rawFeatures[offset + 3]);
        }
    }

    /**
     * @brief Xóa trắng toàn bộ bộ đệm vòng của các kênh
     */
    void clear()
    {
        for (size_t c = 0; c < MAX_CHANNELS; c++)
        {
            _buffers[c].clear();
        }
        _winnerLabel = 0;
        _winnerProb = 0.0f;
        _lastExecTimeUs = 0;
    }
};

#endif /* EDGE_AI_HPP_ */