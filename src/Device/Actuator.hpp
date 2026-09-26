#ifndef AIOT_DEVICE_ACTUATOR_HPP
#define AIOT_DEVICE_ACTUATOR_HPP

#include <Arduino.h>

#if defined(ESP32)
#include "driver/rmt.h"
#endif

struct RelayDescriptor
{
    int pin;
    char name[24];
    bool state;
    bool activeLow;
};

class ActuatorManager
{
public:
    static const size_t MAX_RELAYS = 16;

    ActuatorManager() : _ledPin(-1), _buzzerPin(-1), _rgbPin(-1), _relayCount(0)
    {
        for (size_t i = 0; i < MAX_RELAYS; i++)
        {
            _relays[i].pin = -1;
            _relays[i].name[0] = '\0';
            _relays[i].state = false;
            _relays[i].activeLow = false;
        }
    }

    // ==========================================================
    // 1. QUẢN LÝ RELAY ĐỘNG THEO GPIO PIN (KHÔNG CỐ ĐỊNH CHÂN)
    // ==========================================================

    // Đăng ký Relay vào hệ thống kèm nhãn định danh cho AI / Cloud
    int attachRelay(int pin, const char *name = "Relay", bool activeLow = false)
    {
        if (pin < 0)
            return -1;

        // Nếu pin đã đăng ký trước đó, chỉ cần cập nhật tên và cấu hình
        for (size_t i = 0; i < _relayCount; i++)
        {
            if (_relays[i].pin == pin)
            {
                if (name && strlen(name) > 0)
                {
                    strncpy(_relays[i].name, name, sizeof(_relays[i].name) - 1);
                    _relays[i].name[sizeof(_relays[i].name) - 1] = '\0';
                }
                _relays[i].activeLow = activeLow;
                return (int)i;
            }
        }

        // Đăng ký mới vào danh sách
        if (_relayCount < MAX_RELAYS)
        {
            size_t idx = _relayCount++;
            _relays[idx].pin = pin;
            if (name && strlen(name) > 0)
            {
                strncpy(_relays[idx].name, name, sizeof(_relays[idx].name) - 1);
                _relays[idx].name[sizeof(_relays[idx].name) - 1] = '\0';
            }
            else
            {
                snprintf(_relays[idx].name, sizeof(_relays[idx].name), "Relay_%d", pin);
            }
            _relays[idx].activeLow = activeLow;
            _relays[idx].state = false;

            pinMode(pin, OUTPUT);
            digitalWrite(pin, activeLow ? HIGH : LOW);
            return (int)idx;
        }

        return -1;
    }

    // Điều khiển Relay trực tiếp theo GPIO Pin
    void setRelayByPin(int pin, bool state)
    {
        if (pin < 0)
            return;

        int idx = findRelayIndex(pin);
        if (idx < 0)
        {
            // Tự động gán và khởi tạo pin nếu chưa attach
            idx = attachRelay(pin);
        }

        if (idx >= 0)
        {
            _relays[idx].state = state;
            bool level = _relays[idx].activeLow ? !state : state;
            digitalWrite(pin, level ? HIGH : LOW);
        }
        else
        {
            pinMode(pin, OUTPUT);
            digitalWrite(pin, state ? HIGH : LOW);
        }
    }

    bool getRelayByPin(int pin) const
    {
        if (pin < 0)
            return false;
        int idx = findRelayIndex(pin);
        if (idx >= 0)
        {
            return _relays[idx].state;
        }
        return (digitalRead(pin) == HIGH);
    }

    void toggleRelayByPin(int pin)
    {
        setRelayByPin(pin, !getRelayByPin(pin));
    }

    int findRelayIndex(int pin) const
    {
        for (size_t i = 0; i < _relayCount; i++)
        {
            if (_relays[i].pin == pin)
                return (int)i;
        }
        return -1;
    }

    int getRelayPin(uint8_t index) const
    {
        if (index < _relayCount)
            return _relays[index].pin;
        return -1;
    }

    size_t getRelayCount() const { return _relayCount; }

    const RelayDescriptor *getRelayDescriptor(size_t index) const
    {
        if (index < _relayCount)
            return &_relays[index];
        return nullptr;
    }

    const char *getRelayNameByPin(int pin) const
    {
        int idx = findRelayIndex(pin);
        if (idx >= 0)
            return _relays[idx].name;
        return "Unknown";
    }

    int findRelayByName(const char *name) const
    {
        if (!name)
            return -1;
        for (size_t i = 0; i < _relayCount; i++)
        {
            if (strcasecmp(_relays[i].name, name) == 0)
                return (int)i;
        }
        return -1;
    }

    // ==========================================================
    // 2. TƯƠNG THÍCH NGƯỢC: QUẢN LÝ THEO CHANNEL INDEX (0-INDEXED)
    // ==========================================================
    void setRelayPin(uint8_t index, int pin)
    {
        if (index < MAX_RELAYS)
        {
            if (index >= _relayCount)
                _relayCount = index + 1;

            _relays[index].pin = pin;
            char defaultName[16];
            snprintf(defaultName, sizeof(defaultName), "Relay_%d", index + 1);
            strncpy(_relays[index].name, defaultName, sizeof(_relays[index].name) - 1);
            _relays[index].name[sizeof(_relays[index].name) - 1] = '\0';
            _relays[index].activeLow = false;
            _relays[index].state = false;

            if (pin >= 0)
            {
                pinMode(pin, OUTPUT);
                digitalWrite(pin, LOW);
            }
        }
    }

    void setRelay(uint8_t index, bool state)
    {
        if (index < _relayCount && _relays[index].pin >= 0)
        {
            setRelayByPin(_relays[index].pin, state);
        }
    }

    bool getRelay(uint8_t index) const
    {
        if (index < _relayCount && _relays[index].pin >= 0)
        {
            return _relays[index].state;
        }
        return false;
    }

    void toggleRelay(uint8_t index)
    {
        if (index < _relayCount && _relays[index].pin >= 0)
        {
            setRelay(index, !_relays[index].state);
        }
    }

    // ==========================================================
    // 3. QUẢN LÝ LED, RGB & BUZZER
    // ==========================================================
    void setRgbPin(int pin)
    {
        _rgbPin = pin;
        if (_rgbPin >= 0)
        {
#if defined(ESP32)
            rmt_set_gpio((rmt_channel_t)0, RMT_MODE_TX, (gpio_num_t)_rgbPin, false);
            neopixelWrite(_rgbPin, 0, 0, 0); // Tắt ban đầu
#endif
        }
    }

    int getRgbPin() const { return _rgbPin; }

    void setLedPin(int pin)
    {
        _ledPin = pin;
        if (_ledPin >= 0)
        {
            pinMode(_ledPin, OUTPUT);
            digitalWrite(_ledPin, LOW);
        }
    }

    int getLedPin() const { return _ledPin; }

    void setBuzzerPin(int pin)
    {
        _buzzerPin = pin;
        if (_buzzerPin >= 0)
        {
            pinMode(_buzzerPin, OUTPUT);
            digitalWrite(_buzzerPin, LOW);
        }
    }

    int getBuzzerPin() const { return _buzzerPin; }

    void setLed(bool state)
    {
        if (_ledPin >= 0)
        {
            digitalWrite(_ledPin, state ? HIGH : LOW);
        }
    }

    void toggleLed()
    {
        if (_ledPin >= 0)
        {
            digitalWrite(_ledPin, !digitalRead(_ledPin));
        }
    }

    void buzzerBeep(unsigned int durationMs = 100)
    {
        if (_buzzerPin >= 0)
        {
            digitalWrite(_buzzerPin, HIGH);
            delay(durationMs);
            digitalWrite(_buzzerPin, LOW);
        }
    }

    void setBuzzer(bool state)
    {
        if (_buzzerPin >= 0)
        {
            digitalWrite(_buzzerPin, state ? HIGH : LOW);
        }
    }

private:
    int _ledPin;
    int _buzzerPin;
    int _rgbPin;
    RelayDescriptor _relays[MAX_RELAYS];
    size_t _relayCount;
};

#endif /* AIOT_DEVICE_ACTUATOR_HPP */
