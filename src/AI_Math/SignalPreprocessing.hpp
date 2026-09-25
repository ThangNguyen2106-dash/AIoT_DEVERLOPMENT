#ifndef AI_MATH_SIGNAL_PREPROCESSING_HPP
#define AI_MATH_SIGNAL_PREPROCESSING_HPP

#include <Arduino.h>
#include <math.h>

namespace AI_Math
{
    // =========================================================================
    // GIAI ĐOẠN 1: TIỀN XỬ LÝ TÍN HIỆU (SIGNAL PREPROCESSING)
    // -------------------------------------------------------------------------
    // Mục đích:
    //   - Khử nhiễu, làm mượt tín hiệu chuỗi thời gian từ cảm biến.
    //   - Lưu trữ chuỗi mẫu liên tục vào bộ đệm vòng (Sliding Window / FIFO).
    //   - Chuyển đổi tín hiệu miền thời gian sang phổ tần số (FFT).
    //
    // Đầu vào: Mẫu dữ liệu thô (float rawSample).
    // Đầu ra : Mẫu sạch (float cleanSample) hoặc mảng chuỗi mẫu thời gian thực.
    // =========================================================================

    // =========================================================================
    // 1. CỬA SỔ TRƯỢT (SLIDING WINDOW)
    // -------------------------------------------------------------------------
    // Bộ đệm vòng FIFO (First-In, First-Out) lưu trữ N mẫu liên tục trên RAM tĩnh.
    // Khi đầy, mẫu mới sẽ tự động ghi đè lên mẫu cũ nhất.
    // =========================================================================
    template <size_t CAPACITY>
    class SlidingWindow
    {
    public:
        SlidingWindow() : head(0), count(0) {}

        // Đẩy 1 mẫu dữ liệu mới vào bộ đệm
        void push(float value)
        {
            buffer[head] = value;
            head = (head + 1) % CAPACITY;
            if (count < CAPACITY)
                count++;
        }

        // Xóa toàn bộ dữ liệu trong bộ đệm
        void clear()
        {
            head = 0;
            count = 0;
        }

        // Kiểm tra bộ đệm đã đầy sức chứa hay chưa
        bool isFull() const { return count == CAPACITY; }

        // Số lượng mẫu hiện có trong bộ đệm
        size_t size() const { return count; }

        // Sức chứa tối đa của bộ đệm
        size_t capacity() const { return CAPACITY; }

        // Sao chép chuỗi mẫu ra mảng đích theo thứ tự thời gian (từ cũ nhất đến mới nhất)
        void toArray(float *dest) const
        {
            if (dest == nullptr || count == 0)
                return;
            size_t startIdx = (count < CAPACITY) ? 0 : head;
            for (size_t i = 0; i < count; i++)
            {
                dest[i] = buffer[(startIdx + i) % CAPACITY];
            }
        }

        // Lấy giá trị của mẫu mới nhất vừa được đưa vào
        float latest() const
        {
            if (count == 0)
                return 0.0f;
            size_t idx = (head == 0) ? (CAPACITY - 1) : (head - 1);
            return buffer[idx];
        }

    private:
        float buffer[CAPACITY];
        size_t head;
        size_t count;
    };

    // =========================================================================
    // 2. BỘ LỌC TRUNG BÌNH TRƯỢT (MOVING AVERAGE FILTER)
    // -------------------------------------------------------------------------
    // Công thức: y[n] = (1 / M) * sum(x[n - k]) với k = 0 .. M-1
    // Giảm biên độ dao động ngẫu nhiên bằng cách lấy trung bình của M mẫu gần nhất.
    // =========================================================================
    class MovingAverageFilter
    {
    public:
        // windowSize: Kích thước cửa sổ lấy trung bình (tối đa 32 mẫu)
        MovingAverageFilter(size_t windowSize = 5) : size(windowSize), index(0), count(0), sum(0.0f)
        {
            if (size > 32)
                size = 32;
            for (size_t i = 0; i < 32; i++)
                history[i] = 0.0f;
        }

        // Cập nhật mẫu đầu vào mới và trả về giá trị trung bình sau lọc
        float update(float input)
        {
            sum -= history[index];
            history[index] = input;
            sum += input;
            index = (index + 1) % size;
            if (count < size)
                count++;
            return sum / (float)count;
        }

        // Đặt lại trạng thái bộ lọc
        void reset()
        {
            index = 0;
            count = 0;
            sum = 0.0f;
            for (size_t i = 0; i < 32; i++)
                history[i] = 0.0f;
        }

    private:
        float history[32];
        size_t size;
        size_t index;
        size_t count;
        float sum;
    };

    // =========================================================================
    // 3. BỘ LỌC THÔNG THẤP (EXPONENTIAL IIR LOW-PASS FILTER)
    // -------------------------------------------------------------------------
    // Công thức: y[n] = alpha * x[n] + (1 - alpha) * y[n - 1]
    // Tham số alpha trong dải [0.0, 1.0]:
    //   - alpha càng nhỏ: Khả năng khử nhiễu tần số cao càng mạnh, tín hiệu càng mượt.
    //   - alpha càng lớn: Tín hiệu bám theo đầu vào càng nhanh, độ trễ pha càng nhỏ.
    // =========================================================================
    class LowPassFilter
    {
    public:
        LowPassFilter(float alpha = 0.2f) : _alpha(alpha), _output(0.0f), _initialized(false) {}

        // Cập nhật mẫu đầu vào và trả về giá trị sau lọc
        float update(float input)
        {
            if (!_initialized)
            {
                _output = input;
                _initialized = true;
                return _output;
            }
            _output = _alpha * input + (1.0f - _alpha) * _output;
            return _output;
        }

        void reset() { _initialized = false; }
        void setAlpha(float alpha) { _alpha = alpha; }

    private:
        float _alpha;
        float _output;
        bool _initialized;
    };

    // =========================================================================
    // 4. BỘ LỌC KALMAN 1 CHIỀU (1D KALMAN FILTER)
    // -------------------------------------------------------------------------
    // Thuật toán ước lượng trạng thái tối ưu cho hệ tuyến tính 1 chiều có nhiễu Gauss:
    //   1. Dự đoán (Predict):
    //        x_hat(-) = x_hat
    //        P(-) = P + Q
    //   2. Cập nhật (Update):
    //        K = P(-) / (P(-) + R)
    //        x_hat = x_hat(-) + K * (z - x_hat(-))
    //        P = (1 - K) * P(-)
    //
    // Tham số cấu hình:
    //   - q (Process Noise Covariance): Hiệp phương sai nhiễu quá trình (mức biến thiên thực tế).
    //   - r (Measurement Noise Covariance): Hiệp phương sai nhiễu đo lường của cảm biến.
    // =========================================================================
    class KalmanFilter1D
    {
    public:
        KalmanFilter1D(float q = 0.01f, float r = 0.1f, float p = 1.0f)
            : _q(q), _r(r), _p(p), _x(0.0f), _k(0.0f), _initialized(false)
        {
        }

        // Cập nhật giá trị đo (measurement) và trả về giá trị trạng thái ước lượng tối ưu
        float update(float measurement)
        {
            if (!_initialized)
            {
                _x = measurement;
                _initialized = true;
                return _x;
            }

            // 1. Pha Dự đoán (Time Update / Predict)
            _p += _q;

            // 2. Pha Hiệu chỉnh (Measurement Update / Correct)
            _k = _p / (_p + _r);
            _x += _k * (measurement - _x);
            _p = (1.0f - _k) * _p;

            return _x;
        }

        // Đặt lại bộ lọc với giá trị ước lượng và phương sai ban đầu
        void reset(float initialValue = 0.0f, float p = 1.0f)
        {
            _x = initialValue;
            _p = p;
            _initialized = false;
        }

        void setProcessNoise(float q) { _q = q; }
        void setMeasurementNoise(float r) { _r = r; }
        float getEstimate() const { return _x; }
        float getErrorCovariance() const { return _p; }
        float getKalmanGain() const { return _k; }

    private:
        float _q; // Process noise covariance
        float _r; // Measurement noise covariance
        float _p; // Estimation error covariance
        float _x; // State estimate
        float _k; // Kalman gain
        bool _initialized;
    };

    // =========================================================================
    // 5. BIẾN ĐỔI FOURIER NHANH (COOLEY-TUKEY RADIX-2 FFT)
    // -------------------------------------------------------------------------
    // Chuyển đổi N mẫu tín hiệu thực miền thời gian sang phổ biên độ miền tần số.
    // Yêu cầu: N phải là lũy thừa của 2 (ví dụ: 16, 32, 64, 128, 256).
    // =========================================================================
    class FastFourierTransform
    {
    public:
        // inputReal       : Mảng tín hiệu đầu vào kích thước N
        // outputMagnitude : Mảng biên độ phổ đầu ra kích thước N/2 (cho nửa dải tần [0, Fs/2))
        // N               : Số mẫu (lũy thừa của 2)
        static void computeMagnitude(const float *inputReal, float *outputMagnitude, size_t N)
        {
            if (inputReal == nullptr || outputMagnitude == nullptr || N < 2)
                return;

            float real[N];
            float imag[N];

            for (size_t i = 0; i < N; i++)
            {
                real[i] = inputReal[i];
                imag[i] = 0.0f;
            }

            // Hoán vị đảo bit (Bit-reversal permutation)
            size_t j = 0;
            for (size_t i = 0; i < N - 1; i++)
            {
                if (i < j)
                {
                    float tempR = real[i];
                    real[i] = real[j];
                    real[j] = tempR;

                    float tempI = imag[i];
                    imag[i] = imag[j];
                    imag[j] = tempI;
                }
                size_t k = N >> 1;
                while (k <= j)
                {
                    j -= k;
                    k >>= 1;
                }
                j += k;
            }

            // Thuật toán Cooley-Tukey Radix-2 FFT
            for (size_t len = 2; len <= N; len <<= 1)
            {
                float angle = -2.0f * (float)M_PI / (float)len;
                float wlen_r = cosf(angle);
                float wlen_i = sinf(angle);

                for (size_t i = 0; i < N; i += len)
                {
                    float w_r = 1.0f;
                    float w_i = 0.0f;

                    for (size_t k = 0; k < len / 2; k++)
                    {
                        float u_r = real[i + k];
                        float u_i = imag[i + k];

                        float v_r = real[i + k + len / 2] * w_r - imag[i + k + len / 2] * w_i;
                        float v_i = real[i + k + len / 2] * w_i + imag[i + k + len / 2] * w_r;

                        real[i + k] = u_r + v_r;
                        imag[i + k] = u_i + v_i;

                        real[i + k + len / 2] = u_r - v_r;
                        imag[i + k + len / 2] = u_i - v_i;

                        float temp_w_r = w_r * wlen_r - w_i * wlen_i;
                        w_i = w_r * wlen_i + w_i * wlen_r;
                        w_r = temp_w_r;
                    }
                }
            }

            // Tính biên độ Magnitude cho nửa dải tần [0, N/2)
            for (size_t i = 0; i < N / 2; i++)
            {
                outputMagnitude[i] = sqrtf(real[i] * real[i] + imag[i] * imag[i]) / (float)(N / 2);
            }
        }
    };
}

#endif /* AI_MATH_SIGNAL_PREPROCESSING_HPP */