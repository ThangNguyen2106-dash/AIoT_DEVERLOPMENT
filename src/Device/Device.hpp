#ifndef AIOT_DEVICE_HPP
#define AIOT_DEVICE_HPP

#include <Arduino.h>

// Tự động load Profile phần cứng dựa trên cờ biên dịch hoặc dùng mặc định
#if defined(BOARD_ESP32_S3_KIT)
#include "Profiles/Board_ESP32_S3_Kit.h"
#elif defined(BOARD_AIOT_INDUSTRIAL)
#include "Profiles/Board_AIoT_Industrial.h"
#elif defined(BOARD_ESP32_CAM)
#include "Profiles/Board_ESP32_CAM.h"
#else
#include "Profiles/Board_Default_ESP32.h"
#endif

#include "Actuator.hpp"
#include "Sensor.hpp"

class AIoTDeviceManager
{
public:
    AIoTDeviceManager() : _initialized(false) {}

    void begin()
    {
        if (_initialized)
            return;

#ifdef PIN_STATUS_LED
        actuators.setLedPin(PIN_STATUS_LED);
#endif

#ifdef PIN_RGB_LED
        actuators.setRgbPin(PIN_RGB_LED);
#elif defined(RGB_BUILTIN)
        actuators.setRgbPin(RGB_BUILTIN);
#endif

#ifdef PIN_BUZZER
        actuators.setBuzzerPin(PIN_BUZZER);
#endif

#ifdef PIN_RELAY_1
        actuators.setRelayPin(0, PIN_RELAY_1);
#endif
#ifdef PIN_RELAY_2
        actuators.setRelayPin(1, PIN_RELAY_2);
#endif
#ifdef PIN_RELAY_3
        actuators.setRelayPin(2, PIN_RELAY_3);
#endif
#ifdef PIN_RELAY_4
        actuators.setRelayPin(3, PIN_RELAY_4);
#endif

#ifdef PIN_ANALOG_1
        sensors.setAnalogPin(0, PIN_ANALOG_1);
#endif
#ifdef PIN_ANALOG_2
        sensors.setAnalogPin(1, PIN_ANALOG_2);
#endif
#ifdef PIN_ANALOG_3
        sensors.setAnalogPin(2, PIN_ANALOG_3);
#endif
#ifdef PIN_ANALOG_4
        sensors.setAnalogPin(3, PIN_ANALOG_4);
#endif

#ifdef PIN_BUTTON_USER
        sensors.setDigitalPin(0, PIN_BUTTON_USER, INPUT_PULLUP);
#endif

        _initialized = true;
    }

    const char *getBoardName() const
    {
#ifdef BOARD_NAME
        return BOARD_NAME;
#else
        return "Generic AIoT Board";
#endif
    }

    // --- Cấu hình phần cứng linh hoạt tại runtime ---
    void setRgbPin(int pin) { actuators.setRgbPin(pin); }
    void setLedPin(int pin) { actuators.setLedPin(pin); }
    void setBuzzerPin(int pin) { actuators.setBuzzerPin(pin); }
    void setRelayPin(uint8_t index, int pin)
    {
        if (index >= 1 && index <= 8)
            actuators.setRelayPin(index - 1, pin);
    }
    void setAnalogPin(uint8_t index, int pin)
    {
        if (index >= 1 && index <= 8)
            sensors.setAnalogPin(index - 1, pin);
    }
    void setDigitalPin(uint8_t index, int pin, int mode = INPUT)
    {
        if (index >= 1 && index <= 8)
            sensors.setDigitalPin(index - 1, pin, mode);
    }

    // Điều khiển Relay (1-indexed: relay(1, true))
    void relay(uint8_t index, bool state)
    {
        if (index >= 1 && index <= 8)
        {
            actuators.setRelay(index - 1, state);
        }
    }

    bool getRelay(uint8_t index) const
    {
        if (index >= 1 && index <= 8)
        {
            return actuators.getRelay(index - 1);
        }
        return false;
    }

    void toggleRelay(uint8_t index)
    {
        if (index >= 1 && index <= 8)
        {
            actuators.toggleRelay(index - 1);
        }
    }

    void led(bool state)
    {
        actuators.setLed(state);
#if defined(ESP32)
        // Chỉ fallback sang đèn RGB nếu thiết bị không có chân RGB chuyên dụng
        if (actuators.getRgbPin() < 0)
        {
            if (state)
                rgb(64, 64, 64);
            else
                rgb(0, 0, 0);
        }
#endif
    }

    void rgb(uint8_t r, uint8_t g, uint8_t b)
    {
#if defined(ESP32)
        int rgbPin = actuators.getRgbPin();
        if (rgbPin >= 0)
        {
            rmt_set_gpio((rmt_channel_t)0, RMT_MODE_TX, (gpio_num_t)rgbPin, false);
            neopixelWrite(rgbPin, r, g, b);
        }

#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(BOARD_ESP32_S3_KIT)
        // ESP32-S3 DevKit có 2 phiên bản chân RGB onboard phổ biến: Rev 1.0 (GPIO 48) và Rev 1.1 (GPIO 38).
        // Tự động phát xung ra cả 2 chân để đảm bảo 100% board nào cũng sáng đèn!
        if (rgbPin == 48)
        {
            rmt_set_gpio((rmt_channel_t)0, RMT_MODE_TX, (gpio_num_t)38, false);
            neopixelWrite(38, r, g, b);
            rmt_set_gpio((rmt_channel_t)0, RMT_MODE_TX, (gpio_num_t)48, false);
        }
        else if (rgbPin == 38)
        {
            rmt_set_gpio((rmt_channel_t)0, RMT_MODE_TX, (gpio_num_t)48, false);
            neopixelWrite(48, r, g, b);
            rmt_set_gpio((rmt_channel_t)0, RMT_MODE_TX, (gpio_num_t)38, false);
        }
#endif
#endif
    }

    void toggleLed() { actuators.toggleLed(); }
    void buzzer(bool state) { actuators.setBuzzer(state); }
    void beep(unsigned int ms = 100) { actuators.buzzerBeep(ms); }

    int readAnalog(uint8_t index = 1) const
    {
        if (index >= 1 && index <= 8)
            return sensors.readAnalog(index - 1);
        return 0;
    }

    float readVoltage(uint8_t index = 1) const
    {
        if (index >= 1 && index <= 8)
            return sensors.readAnalogVoltage(index - 1);
        return 0.0f;
    }

    bool readButton(uint8_t index = 1) const
    {
        if (index >= 1 && index <= 8)
            return sensors.readDigital(index - 1);
        return false;
    }

    float readChipTemp() const { return SensorManager::readChipTemperature(); }
    uint32_t readFreeRam() const { return SensorManager::readFreeRam(); }

    ActuatorManager actuators;
    SensorManager sensors;

private:
    bool _initialized;
};

#if __cplusplus >= 201703L
inline AIoTDeviceManager AIoT_Device;
#else
extern AIoTDeviceManager AIoT_Device;
#endif

#endif /* AIOT_DEVICE_HPP */

