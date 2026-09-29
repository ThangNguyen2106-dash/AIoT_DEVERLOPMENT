#ifndef AIOT_DEVICE_SENSOR_HPP
#define AIOT_DEVICE_SENSOR_HPP

#include <Arduino.h>

class SensorManager
{
public:
    SensorManager()
    {
        for (int i = 0; i < 8; i++)
        {
            _analogPins[i] = -1;
            _digitalPins[i] = -1;
        }
    }

    void setAnalogPin(uint8_t index, int pin)
    {
        if (index < 8)
        {
            _analogPins[index] = pin;
        }
    }

    void setDigitalPin(uint8_t index, int pin, int mode = INPUT)
    {
        if (index < 8)
        {
            _digitalPins[index] = pin;
            if (pin >= 0)
            {
                pinMode(pin, mode);
            }
        }
    }

    /**
     * @brief Đọc giá trị ADC analog từ kênh cảm biến
     * @note Trên ESP32 WROOM-32: Hãy ưu tiên sử dụng các chân ADC1 (GPIO 32 - 39).
     *       Tránh sử dụng ADC2 (GPIO 0, 2, 4, 12-15, 25-27) khi WiFi đang hoạt động vì sẽ bị xung đột phần cứng.
     */
    int readAnalog(uint8_t index) const
    {
        if (index < 8 && _analogPins[index] >= 0)
        {
            return analogRead(_analogPins[index]);
        }
        return 0;
    }

    float readAnalogVoltage(uint8_t index, float vRef = 3.3f, int maxAdc = 4095) const
    {
        if (maxAdc <= 0)
            return 0.0f;
        int raw = readAnalog(index);
        return ((float)raw / (float)maxAdc) * vRef;
    }

    bool readDigital(uint8_t index) const
    {
        if (index < 8 && _digitalPins[index] >= 0)
        {
            return digitalRead(_digitalPins[index]) == HIGH;
        }
        return false;
    }

    // Đọc nhiệt độ tích hợp của chip ESP32 (°C)
    static float readChipTemperature()
    {
#if defined(ESP32)
        return temperatureRead();
#else
        return 0.0f;
#endif
    }

    // Đọc dung lượng RAM khả dụng (bytes)
    static uint32_t readFreeRam()
    {
#if defined(ESP32) || defined(ESP8266)
        return ESP.getFreeHeap();
#else
        return 0;
#endif
    }

private:
    int _analogPins[8];
    int _digitalPins[8];
};

#endif /* AIOT_DEVICE_SENSOR_HPP */

