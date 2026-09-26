#ifndef AI_MATH_SIGNAL_PREPROCESSING_HPP
#define AI_MATH_SIGNAL_PREPROCESSING_HPP

#include <Arduino.h>
#include <math.h>
#include <IoT/DEBUG.hpp>

namespace AI_Math
{
    // =========================================================================
    // GIAI ĐOẠN 1: TIỀN XỬ LÝ TÍN HIỆU (SIGNAL PREPROCESSING)
    // -------------------------------------------------------------------------
    // Cấu trúc:
    //   1. Lớp KalmanFilter: Bộ lọc nhiễu thích nghi tuyến tính.
    //   2. Lớp CircularBuffer: Bộ đệm vòng FIFO (Sliding Window) thu thập dữ liệu.
    // =========================================================================

    class KalmanFilter
    {
    private:
        float _q; // Sai số quy trình (Process Noise Covariance) - Càng nhỏ bộ lọc càng mượt
        float _r; // Sai số đo lường (Measurement Noise Covariance) - Càng lớn bộ lọc càng khử nhiễu mạnh
        float _p; // Ước lượng sai số lỗi (Estimation Error Covariance)
        float _x; // Trạng thái/Giá trị ước lượng hiện tại (Estimated Value)
        float _k; // Hệ số tăng Kalman (Kalman Gain)

    public:
        /**
         * @brief Khởi tạo bộ lọc Kalman
         * @param q Process Noise (Mặc định: 0.01)
         * @param r Measurement Noise (Mặc định: 0.1)
         * @param p Error Covariance ban đầu (Mặc định: 1.0)
         * @param initial_value Giá trị khởi tạo nền ban đầu (Mặc định: 0.0)
         */
        KalmanFilter(float q = 0.01f, float r = 0.1f, float p = 1.0f, float initial_value = 0.0f)
            : _q(q), _r(r), _p(p), _x(initial_value), _k(0.0f) {}

        /**
         * @brief Cập nhật và lọc mẫu dữ liệu mới
         * @param rawSample Giá trị thô đọc trực tiếp từ cảm biến
         * @return Giá trị sạch sau khi qua bộ lọc Kalman
         */
        float update(float rawSample)
        {
            // Giai đoạn 1: Dự đoán (Predict)
            _p = _p + _q;

            // Giai đoạn 2: Cập nhật (Update)
            _k = _p / (_p + _r);
            _x = _x + _k * (rawSample - _x);
            _p = (1.0f - _k) * _p;

            return _x;
        }

        // Các hàm Getter/Setter giúp cấu hình động độ nhạy từ xa qua Python GUI
        void setParameters(float q, float r)
        {
            if (q <= 0.0f || r <= 0.0f)
            {
                LOG_WARN("AI_FILTER", "Kalman parameters must be > 0 (q=%.4f, r=%.4f)", q, r);
            }
            _q = q;
            _r = r;
        }
        float getCleanValue() const { return _x; }
    };

    template <size_t WindowSize>
    class CircularBuffer
    {
    private:
        float _buffer[WindowSize];
        size_t _head;
        size_t _count;

    public:
        CircularBuffer() : _head(0), _count(0)
        {
            for (size_t i = 0; i < WindowSize; i++)
            {
                _buffer[i] = 0.0f;
            }
        }

        /**
         * @brief Đẩy một mẫu dữ liệu mới vào bộ đệm (Sliding Window)
         * @param sample Mẫu dữ liệu (Thường là mẫu sạch đã qua Kalman)
         */
        void push(float sample)
        {
            _buffer[_head] = sample;
            _head = (_head + 1) % WindowSize;
            if (_count < WindowSize)
            {
                _count++;
            }
        }

        /**
         * @brief Lấy mẫu dữ liệu tại một vị trí index cụ thể
         * @param index Vị trí cần lấy (0 là mẫu cũ nhất, count-1 là mẫu mới nhất)
         */
        float get(size_t index) const
        {
            if (index >= _count)
                return 0.0f;
            // Tính toán vị trí thực tế trong bộ đệm vòng FIFO
            size_t realIndex = (_count == WindowSize) ? (_head + index) % WindowSize : index;
            return _buffer[realIndex];
        }

        /**
         * @brief Kiểm tra xem bộ đệm đã thu thập đủ dữ liệu chưa
         */
        bool isFull() const { return _count == WindowSize; }

        /**
         * @brief Lấy số lượng mẫu hiện tại đang có trong bộ đệm
         */
        size_t size() const { return _count; }

        /**
         * @brief Xuất toàn bộ dữ liệu hiện tại theo thứ tự từ cũ đến mới ra mảng đích
         * @param dest Con trỏ mảng đầu ra (Kích thước tối thiểu >= size())
         */
        void toArray(float *dest) const
        {
            if (dest == nullptr)
            {
                LOG_ERROR("AI_FILTER", "CircularBuffer::toArray called with NULL pointer!");
                return;
            }
            for (size_t i = 0; i < _count; i++)
            {
                dest[i] = get(i);
            }
        }

        /**
         * @brief Xóa sạch bộ đệm về trạng thái ban đầu
         */
        void clear()
        {
            _head = 0;
            _count = 0;
        }
    };
}

#endif /* AI_MATH_SIGNAL_PREPROCESSING_HPP */