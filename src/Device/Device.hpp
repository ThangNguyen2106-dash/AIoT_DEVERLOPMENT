#ifndef AIOT_DEVICE_HPP
#define AIOT_DEVICE_HPP

#include <Arduino.h>
#include "Actuator.hpp"
#include "Sensor.hpp"

class AIoTDeviceManager;

// ======================================================
// RELAY HANDLE: Cho phép thao tác hướng đối tượng linh hoạt:
// AIoT_Device.Relay(pin).on();
// AIoT_Device.Relay(pin).off();
// AIoT_Device.Relay(pin).toggle();
// AIoT_Device.Relay(pin) = true;
// bool state = AIoT_Device.Relay(pin);
// ======================================================
class RelayHandle
{
public:
    RelayHandle(int pin, AIoTDeviceManager *mgr) : _pin(pin), _mgr(mgr) {}

    void on();
    void off();
    void toggle();
    void set(bool state);
    bool getState() const;
    bool state() const { return getState(); }
    const char *name() const;

    operator bool() const { return getState(); }
    RelayHandle &operator=(bool state)
    {
        set(state);
        return *this;
    }

private:
    int _pin;
    AIoTDeviceManager *_mgr;
};

// ======================================================
// AIoT DEVICE MANAGER: Quản lý thiết bị độc lập phần cứng
// Hoàn toàn không cố định chân - Người dùng tự do cấu hình theo nhu cầu
// ======================================================
class AIoTDeviceManager
{
public:
    AIoTDeviceManager() : _initialized(false) {}

    void begin()
    {
        _initialized = true;
    }

    // --- 1. ĐĂNG KÝ THIẾT BỊ ĐỘNG CHO AI & CLOUD NHẬN BIẾT ---
    int attachRelay(int pin, const char *name = "Relay", bool activeLow = false)
    {
        return actuators.attachRelay(pin, name, activeLow);
    }

    // --- 2. CÚ PHÁP ĐIỀU KHIỂN & ĐĂNG KÝ RELAY LINH HOẠT THEO PIN ---
    // Khởi tạo / đăng ký nhanh kèm tên nhãn cho AI:
    // AIoT_Device.Relay(14, "Quạt chính");
    int Relay(int pin, const char *name, bool activeLow = false)
    {
        return attachRelay(pin, name, activeLow);
    }

    // Đối tượng thao tác OOP: AIoT_Device.Relay(14).on();
    RelayHandle Relay(int pin)
    {
        return RelayHandle(pin, this);
    }

    // Điều khiển ngắn gọn: AIoT_Device.Relay(14, true);
    void Relay(int pin, bool state)
    {
        relay(pin, state);
    }

    // Điều khiển Relay (pin hoặc channel index)
    void relay(int pinOrIndex, bool state)
    {
        // Nếu pinOrIndex trong khoảng 1..8 và đã từng được cấu hình qua setRelayPin
        if (pinOrIndex >= 1 && pinOrIndex <= 8 && actuators.getRelayPin((uint8_t)(pinOrIndex - 1)) >= 0)
        {
            actuators.setRelay((uint8_t)(pinOrIndex - 1), state);
        }
        else
        {
            actuators.setRelayByPin(pinOrIndex, state);
        }
    }

    bool getRelay(int pinOrIndex) const
    {
        if (pinOrIndex >= 1 && pinOrIndex <= 8 && actuators.getRelayPin((uint8_t)(pinOrIndex - 1)) >= 0)
        {
            return actuators.getRelay((uint8_t)(pinOrIndex - 1));
        }
        return actuators.getRelayByPin(pinOrIndex);
    }

    void toggleRelay(int pinOrIndex)
    {
        relay(pinOrIndex, !getRelay(pinOrIndex));
    }

    // Cấu hình chân relay theo index (1-based)
    void setRelayPin(uint8_t index, int pin)
    {
        if (index >= 1 && index <= 16)
        {
            actuators.setRelayPin(index - 1, pin);
        }
    }

    // --- 3. ĐIỀU KHIỂN ĐÈN LED, RGB & CÒI BUZZER ---
    void setRgbPin(int pin) { actuators.setRgbPin(pin); }
    void setLedPin(int pin) { actuators.setLedPin(pin); }
    void setBuzzerPin(int pin) { actuators.setBuzzerPin(pin); }

    void led(bool state)
    {
        actuators.setLed(state);
#if defined(ESP32)
        if (actuators.getRgbPin() < 0)
        {
            if (state)
                rgb(64, 64, 64);
            else
                rgb(0, 0, 0);
        }
#endif
    }

    void toggleLed() { actuators.toggleLed(); }
    void buzzer(bool state) { actuators.setBuzzer(state); }
    void beep(unsigned int ms = 100) { actuators.buzzerBeep(ms); }

    void rgb(uint8_t r, uint8_t g, uint8_t b)
    {
#if defined(ESP32)
        int rgbPin = actuators.getRgbPin();
        if (rgbPin >= 0)
        {
            rmt_set_gpio((rmt_channel_t)0, RMT_MODE_TX, (gpio_num_t)rgbPin, false);
            neopixelWrite(rgbPin, r, g, b);
        }
#if defined(CONFIG_IDF_TARGET_ESP32S3)
        // Hỗ trợ tự động fallback nếu người dùng dùng board ESP32-S3 DevKit Rev 1.0 (48) hoặc Rev 1.1 (38)
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

    // --- 4. CẢM BIẾN (ANALOG, DIGITAL, CHIP TEMP, FREE RAM) ---
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

    int readAnalog(int pinOrIndex = 1) const
    {
        // Nếu là pin GPIO trực tiếp
        if (pinOrIndex > 8)
            return analogRead(pinOrIndex);
        return sensors.readAnalog(pinOrIndex - 1);
    }

    float readVoltage(int pinOrIndex = 1, float vRef = 3.3f, int maxAdc = 4095) const
    {
        int raw = readAnalog(pinOrIndex);
        return ((float)raw / (float)maxAdc) * vRef;
    }

    bool readButton(int pinOrIndex = 1) const
    {
        if (pinOrIndex > 8)
            return digitalRead(pinOrIndex) == HIGH;
        return sensors.readDigital(pinOrIndex - 1);
    }

    float readChipTemp() const { return SensorManager::readChipTemperature(); }
    uint32_t readFreeRam() const { return SensorManager::readFreeRam(); }

    // --- 5. TỰ ĐỘNG TẠO PROMPT PHẦN CỨNG ĐỘNG CHO CLOUD AI (GEMINI) ---
    // Giúp Gemini LLM và Edge AI luôn hiểu đúng chân GPIO thực tế đang chạy
    String getHardwarePrompt() const
    {
        String desc = "Hệ thống phần cứng điều khiển đang cấu hình gồm có:\n";
        size_t count = actuators.getRelayCount();
        for (size_t i = 0; i < count; i++)
        {
            const RelayDescriptor *r = actuators.getRelayDescriptor(i);
            if (r && r->pin >= 0)
            {
                desc += "- Rơ-le \"";
                desc += r->name;
                desc += "\" (chân GPIO " + String(r->pin) + "): ";
                desc += "Bật [CMD:RELAY:" + String(r->pin) + ":ON] hoặc [CMD:RELAY" + String(i + 1) + "_ON], ";
                desc += "Tắt [CMD:RELAY:" + String(r->pin) + ":OFF] hoặc [CMD:RELAY" + String(i + 1) + "_OFF]\n";
            }
        }
        if (actuators.getLedPin() >= 0)
        {
            desc += "- Đèn LED (chân GPIO " + String(actuators.getLedPin()) + "): Bật [CMD:LED_ON], Tắt [CMD:LED_OFF], Chớp [CMD:LED_BLINK]\n";
        }
        if (actuators.getRgbPin() >= 0)
        {
            desc += "- Đèn RGB (chân GPIO " + String(actuators.getRgbPin()) + "): Đổi màu [CMD:RGB:R,G,B]\n";
        }
        if (actuators.getBuzzerPin() >= 0)
        {
            desc += "- Còi Buzzer (chân GPIO " + String(actuators.getBuzzerPin()) + "): Bíp [CMD:BEEP]\n";
        }
        return desc;
    }

    const char *getRelayName(int pin) const
    {
        return actuators.getRelayNameByPin(pin);
    }

    // Tự động phân tích và kích hoạt chân Relay từ chuỗi lệnh AI trả về
    bool executeCommand(String &text)
    {
        bool handled = false;
        // Bắt cú pháp lệnh chuẩn linh hoạt: [CMD:RELAY:<pin_hoặc_tên>:ON|OFF]
        int idx = 0;
        while ((idx = text.indexOf("[CMD:RELAY:", idx)) != -1)
        {
            int endBracket = text.indexOf("]", idx);
            if (endBracket != -1)
            {
                String tag = text.substring(idx + 11, endBracket);
                int colon = tag.indexOf(':');
                if (colon != -1)
                {
                    String target = tag.substring(0, colon);
                    String action = tag.substring(colon + 1);
                    target.trim();
                    action.toUpperCase();

                    int pin = target.toInt();
                    if (pin > 0 || target == "0")
                    {
                        relay(pin, action == "ON");
                        handled = true;
                    }
                    else
                    {
                        int fIdx = actuators.findRelayByName(target.c_str());
                        if (fIdx >= 0)
                        {
                            actuators.setRelay((uint8_t)fIdx, action == "ON");
                            handled = true;
                        }
                    }
                }
                text.remove(idx, endBracket - idx + 1);
            }
            else
            {
                break;
            }
        }
        return handled;
    }

    ActuatorManager actuators;
    SensorManager sensors;

private:
    bool _initialized;
};

// Triển khai các phương thức của RelayHandle sau khi AIoTDeviceManager đã định nghĩa
inline void RelayHandle::on() { _mgr->relay(_pin, true); }
inline void RelayHandle::off() { _mgr->relay(_pin, false); }
inline void RelayHandle::toggle() { _mgr->toggleRelay(_pin); }
inline void RelayHandle::set(bool state) { _mgr->relay(_pin, state); }
inline bool RelayHandle::getState() const { return _mgr->getRelay(_pin); }
inline const char *RelayHandle::name() const { return _mgr->getRelayName(_pin); }

#if __cplusplus >= 201703L
inline AIoTDeviceManager AIoT_Device;
#else
extern AIoTDeviceManager AIoT_Device;
#endif

#endif /* AIOT_DEVICE_HPP */
