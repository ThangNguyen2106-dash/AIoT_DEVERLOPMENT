#include <Arduino.h>

// #define DEBUG_COLOR
#define BUTTON_CONFIG

#include <AIoT.h>
// ======================================================
// ĐỊNH NGHĨA CHÂN PHẦN CỨNG THEO YÊU CẦU:
// ======================================================
#define PIN_DHT11 5         // Cảm biến nhiệt độ & độ ẩm DHT11 cắm ở chân 5
#define PIN_POTENTIOMETER 6 // Cảm biến giả lập bằng biến trở cắm ở chân 6 (ADC1_CH5)
#define PIN_RELAY1 14       // Chân kích Relay 1 cắm ở chân 14 (Tải 1 / Quạt chính)
#define PIN_RELAY2 15       // Chân kích Relay 2 cắm ở chân 15 (Tải 2 / Máy bơm / Thiết bị phụ)
#define PIN_RGB_LED 48      // LED RGB onboard trên ESP32-S3

// WiFi credentials (hoặc để trống để cấu hình qua Smart Captive Portal Web)
const char *WIFI_SSID = "";
const char *WIFI_PASS = "";

// Tài khoản HiveMQ Cloud
const char *MQTT_USER = "IoT_TEST";
const char *MQTT_PASS = "mt21062005";

// Gemini API Key (Lấy tại aistudio.google.com)
// Tự động nạp từ secrets.h (được .gitignore bảo vệ, không bao giờ đẩy lên Git để tránh bị Google hủy Key)
#if __has_include("secrets.h")
#include "secrets.h"
#else
const char *GEMINI_API_KEY = "YOUR_GEMINI_API_KEY";
#endif

HybridAIEngine hybridAI;

// ======================================================
// TRÌNH ĐỌC CẢM BIẾN DHT11 TỐI ƯU CHO ESP32-S3 (KHÔNG CẦN CÀI THƯ VIỆN NGOÀI)
// ======================================================
class DHT11Sensor
{
public:
    DHT11Sensor(uint8_t pin) : _pin(pin), _lastReadMs(0), _temperature(0.0f), _humidity(0.0f), _lastSuccess(false) {}

    bool read(float &tempOut, float &humOut)
    {
        unsigned long now = millis();
        // DHT11 cần khoảng cách tối thiểu 1.5 - 2s giữa các lần đọc
        if (now - _lastReadMs < 2000 && _lastReadMs != 0)
        {
            tempOut = _temperature;
            humOut = _humidity;
            return false; // Chưa tới chu kỳ đọc mẫu mới từ phần cứng
        }

        _lastReadMs = now;
        uint8_t data[5] = {0, 0, 0, 0, 0};

        // 1. Gửi xung Start: Kéo LOW ít nhất 18-20ms
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, LOW);
        delay(20);

        // Thả bus sang INPUT_PULLUP và khóa ngắt để đo xung chính xác tuyệt đối
        pinMode(_pin, INPUT_PULLUP);
        delayMicroseconds(35);

        noInterrupts();

        // 2. Đợi DHT11 phản hồi (kéo LOW ~80us, rồi kéo HIGH ~80us)
        unsigned long t = micros();
        while (digitalRead(_pin) == HIGH)
        {
            if (micros() - t > 150)
            {
                interrupts();
                _lastSuccess = false;
                tempOut = _temperature;
                humOut = _humidity;
                return false;
            }
        }
        t = micros();
        while (digitalRead(_pin) == LOW)
        {
            if (micros() - t > 150)
            {
                interrupts();
                _lastSuccess = false;
                tempOut = _temperature;
                humOut = _humidity;
                return false;
            }
        }
        t = micros();
        while (digitalRead(_pin) == HIGH)
        {
            if (micros() - t > 150)
            {
                interrupts();
                _lastSuccess = false;
                tempOut = _temperature;
                humOut = _humidity;
                return false;
            }
        }

        // 3. Đọc 40 bits dữ liệu
        for (int i = 0; i < 40; i++)
        {
            unsigned long tLow = micros();
            while (digitalRead(_pin) == LOW)
            {
                if (micros() - tLow > 150)
                {
                    interrupts();
                    _lastSuccess = false;
                    tempOut = _temperature;
                    humOut = _humidity;
                    return false;
                }
            }

            unsigned long tHighStart = micros();
            while (digitalRead(_pin) == HIGH)
            {
                if (micros() - tHighStart > 150)
                    break;
            }
            unsigned long highDuration = micros() - tHighStart;

            // Xung HIGH > 40µs là bit 1, < 40µs là bit 0
            if (highDuration > 40)
            {
                data[i / 8] |= (1 << (7 - (i % 8)));
            }
        }
        interrupts();

        // 4. Kiểm tra Checksum
        uint8_t sum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
        if (data[4] == sum && (data[0] != 0 || data[2] != 0))
        {
            _humidity = (float)data[0] + (float)data[1] * 0.1f;
            _temperature = (float)data[2] + (float)data[3] * 0.1f;
            _lastSuccess = true;
            tempOut = _temperature;
            humOut = _humidity;
            return true;
        }

        _lastSuccess = false;
        tempOut = _temperature;
        humOut = _humidity;
        return false;
    }

    float getTemperature() const { return _temperature; }
    float getHumidity() const { return _humidity; }
    bool isOk() const { return _lastSuccess; }

private:
    uint8_t _pin;
    unsigned long _lastReadMs;
    float _temperature;
    float _humidity;
    bool _lastSuccess;
};

// ======================================================
// TRÌNH ĐỌC CẢM BIẾN GIẢ LẬP QUA BIẾN TRỞ (GPIO 6 - ADC1_CH5)
// ======================================================
class PotentiometerSensor
{
public:
    PotentiometerSensor(uint8_t pin)
        : _pin(pin), _raw(0), _voltage(0.0f), _percent(0.0f), _lastUpdateMs(0) {}

    void begin()
    {
        pinMode(_pin, INPUT);
    }

    void update()
    {
        unsigned long now = millis();
        if (now - _lastUpdateMs < 50 && _lastUpdateMs != 0)
            return; // Đọc tối đa 20 lần/giây để tiết kiệm CPU
        _lastUpdateMs = now;

        // Lấy trung bình 4 lần đọc ADC để lọc nhiễu đường dây
        uint32_t sum = 0;
        for (int i = 0; i < 4; i++)
        {
            sum += analogRead(_pin);
            delayMicroseconds(30);
        }
        _raw = (int)(sum / 4);
        _voltage = (_raw / 4095.0f) * 3.3f;
        _percent = (_raw / 4095.0f) * 100.0f;
    }

    int getRaw() const { return _raw; }
    float getVoltage() const { return _voltage; }
    float getPercent() const { return _percent; }

private:
    uint8_t _pin;
    int _raw;
    float _voltage;
    float _percent;
    unsigned long _lastUpdateMs;
};

static DHT11Sensor dhtSensor(PIN_DHT11);
static PotentiometerSensor potSensor(PIN_POTENTIOMETER);
static float currentTemp = 0.0f;
static float currentHum = 0.0f;
static float currentPotPercent = 0.0f;
static float currentPotVolt = 0.0f;
static int currentPotRaw = 0;
static bool simulateSensorAnomaly = false;

// ======================================================
// CẤU HÌNH PROMPT CHO CLOUD AI (GEMINI):
// Dạy cho Gemini biết phần cứng, quy tắc điều khiển và phong cách trả lời thông minh
// ======================================================
const char *AI_SYSTEM_PROMPT =
    "Bạn là trợ lý AI thông minh tích hợp trên bo mạch ESP32-S3 thuộc hệ thống Hybrid AIoT.\n"
    "Hệ thống phần cứng điều khiển gồm có:\n"
    "- Rơ-le 1 (Relay 1 / Quạt / Tải chính) gắn tại chân GPIO 14: Bật [CMD:RELAY1_ON], Tắt [CMD:RELAY1_OFF]\n"
    "- Rơ-le 2 (Relay 2 / Máy bơm / Van tưới / Tải phụ) gắn tại chân GPIO 15: Bật [CMD:RELAY2_ON], Tắt [CMD:RELAY2_OFF]\n"
    "- Đèn LED đơn (onboard): Bật [CMD:LED_ON], Tắt [CMD:LED_OFF], Chớp nháy [CMD:LED_BLINK]\n"
    "- Đèn LED RGB onboard (chân GPIO 48): Đổi màu [CMD:RGB:R,G,B] (ví dụ: [CMD:RGB:255,0,0] đỏ, [CMD:RGB:0,255,0] xanh lá, [CMD:RGB:0,0,255] xanh lam)\n"
    "- Còi Buzzer: Bíp cảnh báo [CMD:BEEP]\n"
    "- Cân chỉnh Baseline Edge AI: [CMD:CALIBRATE]\n"
    "- Dạy Edge AI nhiệt độ an toàn: [CMD:TEACH]\n"
    "\n"
    "QUY TẮC PHẢN HỒI (CỰC KỲ QUAN TRỌNG):\n"
    "1. Khi người dùng trò chuyện tự do, hỏi thăm, hỏi kiến thức, kể chuyện, làm thơ:\n"
    "   -> Hãy trả lời tự nhiên, thân thiện, vui vẻ, không nịnh nọt. TUYỆT ĐỐI KHÔNG tự tiện đề cập đến nhiệt độ, độ ẩm hay thông số cảm biến nếu người dùng KHÔNG hỏi đến chúng!\n"
    "2. Chỉ khi người dùng hỏi về nhiệt độ, độ ẩm, biến trở, tình trạng phòng/thiết bị, hoặc khi phát hiện sự cố khẩn cấp:\n"
    "   -> Mới sử dụng các số liệu cảm biến trong ngữ cảnh để phân tích và báo cáo.\n"
    "3. Khi người dùng yêu cầu bật/tắt thiết bị (quạt, bơm, đèn, relay,...):\n"
    "   -> Trả lời ngắn gọn, kèm đúng thẻ lệnh [CMD:...] tương ứng.\n"
    "4. Khi người dùng chào hỏi, xã giao ('xin chào', 'hello', 'chào bạn', 'hi'):\n"
    "   -> Trả lời thật lịch thiệp, vui tươi và đính kèm thẻ [CMD:LED_BLINK] để chớp nháy đèn chào mừng!";

// ======================================================
// QUẢN LÝ MÀU ĐÈN LED TRẠNG THÁI HỆ THỐNG (3 CẤP ĐỘ EDGE AI)
// ======================================================
enum SystemLedState
{
    SYS_LED_BOOT,    // Khởi động (Xanh dương)
    SYS_LED_NORMAL,  // Bình thường / An toàn (Xanh lá)
    SYS_LED_WARNING, // Bất thường / Cảnh báo nhẹ (Trắng)
    SYS_LED_CRITICAL // Nguy hiểm / Quá nhiệt / Đột biến (Đỏ)
};

static SystemLedState currentLedState = SYS_LED_BOOT;
static unsigned long customLedUntil = 0; // Hẹn giờ cho các lệnh hiệu ứng đèn tạm thời

void setSystemLed(SystemLedState state, bool force = false)
{
    // Luôn ghi nhớ trạng thái hoạt động cốt lõi của hệ thống
    currentLedState = state;

    // Nếu đang trong thời gian hiển thị lệnh tạm thời và không phải trường hợp khẩn cấp, chờ hết hiệu ứng
    if (customLedUntil > 0 && millis() < customLedUntil && state != SYS_LED_CRITICAL && !force)
        return;

    switch (state)
    {
    case SYS_LED_BOOT:
        AIoT_Device.rgb(0, 0, 64); // Sáng xanh dương nhẹ báo hiệu khởi động
        break;
    case SYS_LED_NORMAL:
        AIoT_Device.rgb(0, 48, 0); // SÁNG XANH LÁ: Hệ thống an toàn, bình thường
        break;
    case SYS_LED_WARNING:
        AIoT_Device.rgb(177, 206, 11); // SÁNG TRẮNG: Bất thường / chớm lệch / cảnh báo nhẹ
        break;
    case SYS_LED_CRITICAL:
        AIoT_Device.rgb(255, 0, 0); // SÁNG ĐỎ RỰC: Sự cố nguy hiểm khẩn cấp / Quá nhiệt
        break;
    }
}

// Hàm khôi phục ngay lập tức màu LED về trạng thái hoạt động thực tế (Xanh lá / Trắng / Đỏ)
void restoreSystemLed()
{
    customLedUntil = 0;
    setSystemLed(currentLedState, true);
}

// ======================================================
// BỘ THỰC THI LỆNH PHẦN CỨNG TỪ AI (ACTUATOR EXECUTOR)
// ======================================================
void executeAICommands(String &reply)
{
    // 1. Điều khiển LED Onboard đơn
    if (reply.indexOf("[CMD:LED_ON]") != -1)
    {
        AIoT_Device.led(true);
        customLedUntil = millis() + 3000; // Sáng trong 3s rồi tự trở về màu hệ thống
        Serial.println("[HARDWARE_ACTION]: >>> DA BAT DEN LED (3s sau tu ve mau he thong) <<<");
        reply.replace("[CMD:LED_ON]", "");
    }
    if (reply.indexOf("[CMD:LED_OFF]") != -1)
    {
        reply.replace("[CMD:LED_OFF]", "");
        restoreSystemLed(); // Khôi phục ngay màu trạng thái hệ thống
        Serial.println("[HARDWARE_ACTION]: >>> DA TAT HIEU UNG VA TRO VE MAU TRANG THAI HE THONG <<<");
    }
    if (reply.indexOf("[CMD:LED_BLINK]") != -1)
    {
        Serial.println("[HARDWARE_ACTION]: >>> DANG CHOP NHAY LED CHAO MUNG <<<");
        for (int i = 0; i < 4; i++)
        {
            AIoT_Device.rgb(64, 64, 64);
            delay(120);
            AIoT_Device.rgb(0, 0, 0);
            delay(120);
        }
        reply.replace("[CMD:LED_BLINK]", "");
        restoreSystemLed(); // KET THUC LENH CHOP TAT: QUAY VE NGAY MAU TRANG THAI HOAT DONG
        Serial.println("[HARDWARE_ACTION]: >>> DA KET THUC CHOP TAT -> TRO VE MAU TRANG THAI HOAT DONG <<<");
    }

    // 2. Điều khiển LED RGB Onboard: [CMD:RGB:r,g,b]
    int rgbIdx = reply.indexOf("[CMD:RGB:");
    if (rgbIdx != -1)
    {
        int endIdx = reply.indexOf("]", rgbIdx);
        if (endIdx != -1)
        {
            String rgbStr = reply.substring(rgbIdx + 9, endIdx);
            int comma1 = rgbStr.indexOf(',');
            int comma2 = rgbStr.indexOf(',', comma1 + 1);
            if (comma1 != -1 && comma2 != -1)
            {
                uint8_t r = rgbStr.substring(0, comma1).toInt();
                uint8_t g = rgbStr.substring(comma1 + 1, comma2).toInt();
                uint8_t b = rgbStr.substring(comma2 + 1).toInt();
                AIoT_Device.rgb(r, g, b);
                customLedUntil = millis() + 3000; // Giữ màu này trong 3 giây rồi tự động trở về màu hệ thống
                Serial.printf("[HARDWARE_ACTION]: >>> DOI MAU LED RGB (%d, %d, %d) TRONG 3S (Sau do tu ve mau he thong) <<<\n", r, g, b);
            }
            reply.remove(rgbIdx, endIdx - rgbIdx + 1);
        }
    }

    // 3. Điều khiển Relay 1 (chân 14 - Tải 1)
    if (reply.indexOf("[CMD:RELAY1_ON]") != -1)
    {
        AIoT_Device.relay(1, true);
        Serial.println("[HARDWARE_ACTION]: >>> DA BAT RELAY 1 (GPIO 14) <<<");
        reply.replace("[CMD:RELAY1_ON]", "");
    }
    if (reply.indexOf("[CMD:RELAY1_OFF]") != -1)
    {
        AIoT_Device.relay(1, false);
        Serial.println("[HARDWARE_ACTION]: >>> DA TAT RELAY 1 (GPIO 14) <<<");
        reply.replace("[CMD:RELAY1_OFF]", "");
    }

    // 4. Điều khiển Relay 2 (chân 15 - Tải 2 / Máy bơm / Thiết bị thêm mới)
    if (reply.indexOf("[CMD:RELAY2_ON]") != -1)
    {
        AIoT_Device.relay(2, true);
        Serial.println("[HARDWARE_ACTION]: >>> DA BAT RELAY 2 (GPIO 15) <<<");
        reply.replace("[CMD:RELAY2_ON]", "");
    }
    if (reply.indexOf("[CMD:RELAY2_OFF]") != -1)
    {
        AIoT_Device.relay(2, false);
        Serial.println("[HARDWARE_ACTION]: >>> DA TAT RELAY 2 (GPIO 15) <<<");
        reply.replace("[CMD:RELAY2_OFF]", "");
    }

    // 5. Bíp còi
    if (reply.indexOf("[CMD:BEEP]") != -1)
    {
        AIoT_Device.beep(150);
        Serial.println("[HARDWARE_ACTION]: >>> COI BUZZER BIP <<<");
        reply.replace("[CMD:BEEP]", "");
    }

    // 6. Cân chỉnh lại đường cơ sở Edge AI
    if (reply.indexOf("[CMD:CALIBRATE]") != -1)
    {
        hybridAI.autoLearn(30, 3.0f);
        Serial.println("[EDGE_AI_ACTION]: >>> DANG TU CAN CHINH LAI BASELINE CHO CAM BIEN DHT11 <<<");
        reply.replace("[CMD:CALIBRATE]", "");
    }

    // 7. Dạy mẫu hiện tại là bình thường theo lệnh AI
    if (reply.indexOf("[CMD:TEACH]") != -1)
    {
        hybridAI.teachNormal(currentTemp);
        AIoT_Device.beep(100);
        Serial.printf("[EDGE_AI_ACTION]: >>> DA DAY MAU %.1f *C VAO BASELINE THEO LENH AI <<<\n", currentTemp);
        reply.replace("[CMD:TEACH]", "");
    }

    reply.trim();
}

// =========================================================================
// BỘ QUY TẮC DẠY AI PHẢN HỒI & ĐIỀU KHIỂN TỰ ĐỘNG TẠI CHỖ (LOCAL OFFLINE AI)
// =========================================================================
// Đây là nơi bạn "dạy" cho AI tự động trả lời và thực thi các câu lệnh đời thường
// mà KHÔNG CẦN Internet, KHÔNG TỐN TOKEN và phản hồi tức thì dưới 1ms!
// Bạn muốn thêm câu lệnh gì (ví dụ Relay 3, Đèn ngủ, Bơm tưới), chỉ cần thêm "if" ở đây:
bool teachLocalAICommands(String text, String &reply)
{
    text.toLowerCase(); // Chuyển về chữ thường để so sánh không phân biệt hoa/thường

    // -------------------------------------------------------------
    // 0. DẠY LỆNH CHÀO HỎI & CHỚP NHÁY ĐÈN LED (OFFLINE):
    // -------------------------------------------------------------
    if (text.indexOf("xin chào") != -1 || text.indexOf("chào bạn") != -1 ||
        text.indexOf("hello") != -1 || text == "hi" || text.indexOf("chào ai") != -1)
    {
        reply = "Chào bạn! Tôi là trợ lý AI trên ESP32-S3, rất vui được hỗ trợ bạn! [CMD:LED_BLINK]";
        return true;
    }

    // -------------------------------------------------------------
    // 1. DẠY LỆNH ĐIỀU KHIỂN RELAY 1 (TẢI 1 / QUẠT 1):
    // -------------------------------------------------------------
    if ((text.indexOf("relay 1") != -1 || text.indexOf("quạt 1") != -1 || text.indexOf("đèn 1") != -1) &&
        (text.indexOf("bật") != -1 || text.indexOf("mở") != -1 || text.indexOf("kích") != -1))
    {
        reply = "Đã nhận lệnh: Bật Relay 1 (GPIO 14) ngay lập tức! [CMD:RELAY1_ON]";
        return true;
    }
    if ((text.indexOf("relay 1") != -1 || text.indexOf("quạt 1") != -1 || text.indexOf("đèn 1") != -1) &&
        (text.indexOf("tắt") != -1 || text.indexOf("ngắt") != -1 || text.indexOf("đóng") != -1))
    {
        reply = "Đã nhận lệnh: Tắt Relay 1 (GPIO 14) an toàn! [CMD:RELAY1_OFF]";
        return true;
    }

    // -------------------------------------------------------------
    // 2. DẠY LỆNH ĐIỀU KHIỂN RELAY 2 (TẢI 2 / MÁY BƠM / THIẾT BỊ MỚI):
    // -------------------------------------------------------------
    if ((text.indexOf("relay 2") != -1 || text.indexOf("máy bơm") != -1 || text.indexOf("quạt 2") != -1 || text.indexOf("van") != -1) &&
        (text.indexOf("bật") != -1 || text.indexOf("mở") != -1 || text.indexOf("kích") != -1 || text.indexOf("chạy") != -1))
    {
        reply = "Đã nhận lệnh: Kích hoạt Relay 2 (GPIO 15) cấp nguồn cho thiết bị! [CMD:RELAY2_ON]";
        return true;
    }
    if ((text.indexOf("relay 2") != -1 || text.indexOf("máy bơm") != -1 || text.indexOf("quạt 2") != -1 || text.indexOf("van") != -1) &&
        (text.indexOf("tắt") != -1 || text.indexOf("ngắt") != -1 || text.indexOf("dừng") != -1 || text.indexOf("đóng") != -1))
    {
        reply = "Đã nhận lệnh: Tắt Relay 2 (GPIO 15) an toàn! [CMD:RELAY2_OFF]";
        return true;
    }

    // -------------------------------------------------------------
    // 3. DẠY CÂU HỎI VỀ NHIỆT ĐỘ & ĐỘ ẨM PHÒNG:
    // Chỉ kích hoạt khi hỏi về nhiệt độ phòng/cảm biến thực tế, tránh chặn nhầm câu hỏi tổng quát
    // -------------------------------------------------------------
    if ((text.indexOf("nhiệt độ") != -1 || text.indexOf("độ ẩm") != -1 || text.indexOf("dht11") != -1) &&
        (text.indexOf("phòng") != -1 || text.indexOf("hiện tại") != -1 || text.indexOf("bao nhiêu") != -1 ||
         text.indexOf("mấy độ") != -1 || text.indexOf("xem") != -1 || text.indexOf("đo") != -1 ||
         text == "nhiệt độ" || text == "độ ẩm"))
    {
        reply = "Nhiệt độ phòng hiện tại đo được từ DHT11 là " + String(currentTemp, 1) + " °C, độ ẩm không khí đạt " + String(currentHum, 1) + " %.";
        return true;
    }

    // -------------------------------------------------------------
    // 4. DẠY CÂU HỎI VỀ BIẾN TRỞ / ÁP SUẤT GIẢ LẬP:
    // -------------------------------------------------------------
    if (text.indexOf("biến trở") != -1 || text.indexOf("chiết áp") != -1 || text.indexOf("adc") != -1 || text.indexOf("pot") != -1)
    {
        reply = "Cảm biến biến trở GPIO 6 đang ở mức " + String(currentPotPercent, 1) + " % (Điện áp: " + String(currentPotVolt, 2) + " V, ADC: " + String(currentPotRaw) + ").";
        return true;
    }

    // -------------------------------------------------------------
    // 5. DẠY CÂU HỎI KIỂM TRA TRẠNG THÁI CÁC RƠ-LE:
    // -------------------------------------------------------------
    if (text.indexOf("trạng thái relay") != -1 || text.indexOf("kiểm tra relay") != -1 || text.indexOf("xem relay") != -1)
    {
        reply = "Trạng thái tải hiện tại: Relay 1 (GPIO 14) đang " + String(AIoT_Device.getRelay(1) ? "BẬT (ON)" : "TẮT (OFF)") +
                ", Relay 2 (GPIO 15) đang " + String(AIoT_Device.getRelay(2) ? "BẬT (ON)" : "TẮT (OFF)") + ".";
        return true;
    }

    // -------------------------------------------------------------
    // 6. DẠY LỆNH ĐỔI MÀU LED RGB HOẶC BÍP CÒI:
    // -------------------------------------------------------------
    if (text.indexOf("bíp") != -1 || text.indexOf("còi") != -1 || text.indexOf("chuông") != -1)
    {
        reply = "Đã phát tín hiệu còi cảnh báo! [CMD:BEEP]";
        return true;
    }
    if (text.indexOf("led xanh lá") != -1 || text.indexOf("đèn xanh lá") != -1)
    {
        reply = "Đã đổi màu LED sang xanh lá! [CMD:RGB:0,255,0]";
        return true;
    }
    if (text.indexOf("led đỏ") != -1 || text.indexOf("đèn đỏ") != -1)
    {
        reply = "Đã đổi màu LED sang đỏ rực! [CMD:RGB:255,0,0]";
        return true;
    }
    if (text.indexOf("led xanh lam") != -1 || text.indexOf("led xanh dương") != -1)
    {
        reply = "Đã đổi màu LED sang xanh dương! [CMD:RGB:0,0,255]";
        return true;
    }

    // -------------------------------------------------------------
    // 7. DẠY LỆNH HỌC DỮ LIỆU EDGE AI:
    // -------------------------------------------------------------
    if (text.indexOf("dạy") != -1 || text.indexOf("an toàn") != -1 || text.indexOf("học mẫu") != -1)
    {
        reply = "Đã nhận lệnh: Dạy Edge AI ghi nhận mức nhiệt độ hiện tại (" + String(currentTemp, 1) + " °C) là an toàn! [CMD:TEACH]";
        return true;
    }

    // Trả về false nếu không khớp với bất kỳ câu lệnh cục bộ nào
    // -> Hệ thống sẽ tự động chuyển tiếp câu hỏi lên Cloud AI (Gemini) để trả lời thông minh!
    return false;
}

// ======================================================
// NHẬN LỆNH TỪ CLOUD (QUA MQTT)
// ======================================================
Virtual_WRITE(ai_chat)
{
    String aiReply = param.getString();
    executeAICommands(aiReply);

    Serial.println("\n--------------------------------------------------");
    Serial.printf("[CLOUD_AI_AGENT (MQTT)]: %s\n", aiReply.c_str());
    Serial.println("--------------------------------------------------");
}

Virtual_WRITE(relay1)
{
    int state = param.getInt();
    AIoT_Device.relay(1, state ? HIGH : LOW);
    Serial.printf("[ACTUATOR] Relay 1 (GPIO 14): %s\n", state ? "ON" : "OFF");
}

Virtual_WRITE(relay2)
{
    int state = param.getInt();
    AIoT_Device.relay(2, state ? HIGH : LOW);
    Serial.printf("[ACTUATOR] Relay 2 (GPIO 15): %s\n", state ? "ON" : "OFF");
}

// ======================================================
// GỬI TELEMETRY TỔNG HỢP LÊN CLOUD (MỖI 5 GIÂY)
// ======================================================
void sendHybridTelemetry()
{
    if (!AIoT.CheckConnect())
        return;

    float meanVal, rmsVal, p2pVal, stdDevVal;
    hybridAI.edge.extractFeatures(meanVal, rmsVal, p2pVal, stdDevVal);

    // Đồng bộ chỉ số cảm biến DHT11, Biến trở GPIO 6, Relay 1, Relay 2 và Edge AI lên Cloud
    AIoT.updateTelemetry("dht_temp", currentTemp);
    AIoT.updateTelemetry("dht_hum", currentHum);
    AIoT.updateTelemetry("pot_percent", currentPotPercent);
    AIoT.updateTelemetry("pot_volt", currentPotVolt);
    AIoT.updateTelemetry("pot_raw", currentPotRaw);
    AIoT.updateTelemetry("relay1_state", AIoT_Device.getRelay(1) ? 1 : 0);
    AIoT.updateTelemetry("relay2_state", AIoT_Device.getRelay(2) ? 1 : 0);
    AIoT.updateTelemetry("sensor_mean", meanVal);
    AIoT.updateTelemetry("sensor_rms", rmsVal);
    AIoT.updateTelemetry("sensor_p2p", p2pVal);
    AIoT.updateTelemetry("chip_temp", AIoT_Device.readChipTemp());
    AIoT.updateTelemetry("free_ram", (int)AIoT_Device.readFreeRam());
    AIoT.sendTelemetry();
}

// ======================================================
// HIỂN THỊ TRẠNG THÁI VÀ GIAO TIẾP VỚI EDGE AI & CLOUD AI
// ======================================================
void printEdgeAIStatus()
{
    float meanVal, rmsVal, p2pVal, stdDevVal;
    hybridAI.edge.extractFeatures(meanVal, rmsVal, p2pVal, stdDevVal);

    float currentZ = hybridAI.edge.getDetector().getZScore(currentTemp);
    float anomalyScore = hybridAI.edge.getDetector().predictScore(currentTemp);
    EdgeAI::DeviceState st = hybridAI.edge.getClassifier().classify(anomalyScore);

    Serial.println("\n==================== EDGE AI STATUS ====================");
    if (dhtSensor.isOk())
    {
        Serial.printf(" - Cảm biến DHT11 (GPIO %d) : Nhiệt độ: %.1f °C | Độ ẩm: %.1f %%\n", PIN_DHT11, currentTemp, currentHum);
    }
    else
    {
        Serial.printf(" - Cảm biến DHT11 (GPIO %d) : [CHƯA NHẬN DỮ LIỆU - Kiểm tra dây cắm]\n", PIN_DHT11);
    }
    Serial.printf(" - Cảm biến Biến Trở (GPIO 6): Mức: %.1f %% | Điện áp: %.2f V | ADC: %d\n", currentPotPercent, currentPotVolt, currentPotRaw);
    Serial.printf(" - Relay 1 (GPIO 14)        : %s\n", AIoT_Device.getRelay(1) ? "BẬT (ON)" : "TẮT (OFF)");
    Serial.printf(" - Relay 2 (GPIO 15)        : %s\n", AIoT_Device.getRelay(2) ? "BẬT (ON)" : "TẮT (OFF)");
    Serial.printf(" - Baseline Calibrated      : %s (%u/30 samples)\n",
                  hybridAI.edge.getDetector().isCalibrated() ? "YES (Sẵn sàng)" : "CALIBRATING...",
                  hybridAI.edge.getDetector().getSampleCount());
    Serial.printf(" - Baseline Phân Phối Chuẩn : Mean: %.2f °C | StdDev: %.2f °C\n",
                  hybridAI.edge.getDetector().getBaselineMean(),
                  hybridAI.edge.getDetector().getBaselineStdDev());
    Serial.printf(" - Z-Score Hiện Tại         : %.2f sigma (Ngưỡng cảnh báo: 1.80σ, Nguy hiểm: %.2fσ)\n",
                  currentZ, hybridAI.edge.getDetector().getThreshold());
    Serial.printf(" - Điểm Dị Thường (Score)   : %.2f / 1.00\n", anomalyScore);
    Serial.printf(" - Trạng Thái Edge AI       : %s\n", EdgeAI::stateToString(st));
    Serial.printf(" - Động Học 64 Mẫu Trượt    : Mean: %.2f °C | RMS: %.2f | P2P: %.2f °C | StdDev: %.2f\n",
                  meanVal, rmsVal, p2pVal, stdDevVal);

    float feat[4] = {meanVal, rmsVal, p2pVal, stdDevVal};
    EdgeAI::TinyMLResult tRes = hybridAI.edge.getNeuralClassifier().predict(feat, 4);
    Serial.printf(" - TinyML Neural Network    : Lớp: %s | Tin cậy: %.1f%% (Độ trễ: %u µs)\n",
                  EdgeAI::stateToString(tRes.predictedClass), tRes.confidence * 100.0f, tRes.inferenceTimeUs);
    Serial.println("==========================================================\n");
}

void printCloudAIStatus()
{
    Serial.println("\n==================== CLOUD AI STATUS ===================");
    Serial.printf(" - WiFi Status         : %s\n", (WiFi.status() == WL_CONNECTED) ? "CONNECTED" : "DISCONNECTED");
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf(" - IP Address          : %s\n", WiFi.localIP().toString().c_str());
        Serial.printf(" - WiFi RSSI           : %d dBm\n", WiFi.RSSI());
    }
    Serial.printf(" - Gemini API Key      : %s\n", hybridAI.gemini.hasApiKey() ? "CONFIGURED (Sẵn sàng)" : "NOT SET");
    Serial.printf(" - Gemini Model        : %s\n", hybridAI.gemini.getModel());
    Serial.printf(" - MQTT Broker Status  : %s\n", AIoT.CheckConnect() ? "CONNECTED (HiveMQ)" : "CONNECTING...");
    if (dhtSensor.isOk())
    {
        Serial.printf(" - Cảm biến DHT11      : %.1f °C, %.1f %%\n", currentTemp, currentHum);
    }
    else
    {
        Serial.printf(" - Cảm biến DHT11      : [CHƯA NHẬN DỮ LIỆU]\n");
    }
    Serial.printf(" - Biến trở (GPIO 6)   : %.1f %% (%.2f V, ADC: %d)\n", currentPotPercent, currentPotVolt, currentPotRaw);
    Serial.printf(" - Relay 1 (GPIO 14)   : %s\n", AIoT_Device.getRelay(1) ? "ON" : "OFF");
    Serial.printf(" - Relay 2 (GPIO 15)   : %s\n", AIoT_Device.getRelay(2) ? "ON" : "OFF");
    Serial.println("==========================================================\n");
}

// ======================================================
// XỬ LÝ CHAT QUA SERIAL TERMINAL (ECHO THỜI GIAN THỰC)
// ======================================================
static String chatInputBuffer = "";

void handleSerialChat(float sensorValue)
{
    while (Serial.available())
    {
        char c = (char)Serial.read();

        // 1. Khi nhấn Enter (gửi câu hỏi / lệnh)
        if (c == '\n' || c == '\r')
        {
            if (chatInputBuffer.length() > 0)
            {
                Serial.println(); // Xuống dòng trên terminal
                String userText = chatInputBuffer;
                chatInputBuffer = "";
                userText.trim();

                Serial.printf("[USER_INPUT]: %s\n", userText.c_str());

                // LỆNH ĐẶC BIỆT 1: Tra cứu trực tiếp trạng thái Edge AI & DHT11
                if (userText.equalsIgnoreCase("/edge") || userText.equalsIgnoreCase("edge"))
                {
                    printEdgeAIStatus();
                    return;
                }

                // LỆNH ĐẶC BIỆT 1B: Đánh giá trực tiếp mạng nơ-ron TinyML (ESP32-S3 Hardware Inference)
                if (userText.equalsIgnoreCase("/tinyml") || userText.equalsIgnoreCase("tinyml"))
                {
                    float meanVal = 0, rmsVal = 0, p2pVal = 0, stdDevVal = 0;
                    hybridAI.edge.extractFeatures(meanVal, rmsVal, p2pVal, stdDevVal);
                    float feat[4] = {meanVal, rmsVal, p2pVal, stdDevVal};

                    EdgeAI::TinyMLResult tRes = hybridAI.edge.getNeuralClassifier().predict(feat, 4);

                    Serial.println("\n================= TINYML NEURAL INFERENCE (ESP32-S3) ================");
                    Serial.printf("   - Vector đặc trưng (Inputs): [Mean=%.2f, RMS=%.2f, P2P=%.2f, StdDev=%.2f]\n",
                                  meanVal, rmsVal, p2pVal, stdDevVal);
                    Serial.printf("   - Lớp dự đoán (Prediction) : %s\n", EdgeAI::stateToString(tRes.predictedClass));
                    Serial.printf("   - Độ tin cậy (Confidence)  : %.2f %%\n", tRes.confidence * 100.0f);
                    Serial.printf("   - Phân bố xác suất Softmax : [NORMAL: %.1f%%, WARNING: %.1f%%, CRITICAL: %.1f%%]\n",
                                  tRes.probabilities[0] * 100.0f,
                                  tRes.probabilities[1] * 100.0f,
                                  tRes.probabilities[2] * 100.0f);
                    Serial.printf("   - Đầu ra Logits (Dense)    : [%.3f, %.3f, %.3f]\n",
                                  tRes.logits[0], tRes.logits[1], tRes.logits[2]);
                    Serial.printf("   - Thời gian suy luận chip  : %u µs (Microseconds!)\n", tRes.inferenceTimeUs);
                    Serial.println("========================================================================\n");
                    return;
                }

                // LỆNH ĐẶC BIỆT 2: Tra cứu trực tiếp trạng thái Cloud AI
                if (userText.equalsIgnoreCase("/cloud") || userText.equalsIgnoreCase("cloud"))
                {
                    printCloudAIStatus();
                    return;
                }

                // LỆNH ĐẶC BIỆT 2B: Tra cứu trạng thái kết nối Wi-Fi & MQTT (không gây nhiễu màn hình chat)
                if (userText.equalsIgnoreCase("/wifi") || userText.equalsIgnoreCase("wifi") || userText.equalsIgnoreCase("/net"))
                {
                    Serial.println("\n========================================================");
                    Serial.println("   TRẠNG THÁI KẾT NỐI MẠNG (NETWORK STATUS)");
                    Serial.println("--------------------------------------------------------");
                    Serial.printf("   - Wi-Fi Status : %s\n", (WiFi.status() == WL_CONNECTED) ? "CONNECTED [OK]" : "DISCONNECTED [PENDING]");
                    if (WiFi.status() == WL_CONNECTED)
                    {
                        Serial.printf("   - SSID         : %s\n", WiFi.SSID().c_str());
                        Serial.printf("   - IP Address   : %s\n", WiFi.localIP().toString().c_str());
                        Serial.printf("   - Gateway      : %s\n", WiFi.gatewayIP().toString().c_str());
                        Serial.printf("   - Primary DNS  : %s\n", WiFi.dnsIP(0).toString().c_str());
                        Serial.printf("   - Secondary DNS: %s\n", WiFi.dnsIP(1).toString().c_str());
                        Serial.printf("   - Tín hiệu RSSI: %d dBm\n", WiFi.RSSI());
                    }
                    Serial.printf("   - HiveMQ MQTT  : %s\n", (serverMQTT.check_connect()) ? "CONNECTED [OK]" : "DISCONNECTED [FAIL]");
                    Serial.println("========================================================\n");
                    return;
                }

                // LỆNH ĐẶC BIỆT 3: Tự động gom mẫu học lại Baseline (Auto-Learn)
                if (userText.equalsIgnoreCase("/autolearn") || userText.equalsIgnoreCase("/calibrate") || userText.equalsIgnoreCase("calibrate"))
                {
                    hybridAI.autoLearn(30, 3.0f);
                    AIoT_Device.beep(80);
                    Serial.println("[EDGE_AI]: Đang bắt đầu tự động thu thập lại 30 mẫu để học Baseline mới...");
                    return;
                }

                // LỆNH ĐẶC BIỆT 4: Dạy mẫu hiện tại là bình thường (In-flight / Online Teaching)
                if (userText.equalsIgnoreCase("/teach") || userText.equalsIgnoreCase("teach"))
                {
                    hybridAI.teachNormal(sensorValue);
                    AIoT_Device.beep(120);
                    Serial.println("[EDGE_AI_TEACH]: Đã dạy Edge AI ghi nhận giá trị hiện tại là BÌNH THƯỜNG!");
                    Serial.printf("   -> Mẫu nạp: %.1f °C | Baseline Mean: %.2f °C | Dung sai StdDev: %.2f °C\n",
                                  sensorValue,
                                  hybridAI.edge.getDetector().getBaselineMean(),
                                  hybridAI.edge.getDetector().getBaselineStdDev());
                    return;
                }

                // LỆNH ĐẶC BIỆT 5: Dạy trực tiếp mốc chuẩn & dung sai: /teach_base <mean> <std>
                if (userText.startsWith("/teach_base ") || userText.startsWith("teach_base "))
                {
                    String paramStr = userText.substring(userText.indexOf(' ') + 1);
                    paramStr.trim();
                    int sp = paramStr.indexOf(' ');
                    if (sp != -1)
                    {
                        float m = paramStr.substring(0, sp).toFloat();
                        float s = paramStr.substring(sp + 1).toFloat();
                        if (m > 0.0f && s > 0.0f)
                        {
                            hybridAI.teachBaseline(m, s);
                            AIoT_Device.beep(100);
                            Serial.printf("[EDGE_AI_TEACH]: Đã thiết lập mốc chuẩn mới: Mean = %.2f °C, Dung sai StdDev = ±%.2f °C\n", m, s);
                            return;
                        }
                    }
                    Serial.println("Cú pháp: /teach_base <nhiệt_độ_chuẩn> <dung_sai> (Ví dụ: /teach_base 28.5 1.5)");
                    return;
                }

                // LỆNH ĐẶC BIỆT 6: Giả lập đột biến lỗi nhiệt độ cao để test phản xạ Edge AI -> Cloud AI
                if (userText.equalsIgnoreCase("/sim_error") || userText.equalsIgnoreCase("/error") || userText.equalsIgnoreCase("error"))
                {
                    simulateSensorAnomaly = true;
                    Serial.println("[SIMULATION]: Đã kích hoạt 1 xung đột biến nhiệt độ cao (Spike = 85.0 °C)!");
                    Serial.println("   Hãy quan sát Edge AI tự ngắt Relay 1 (GPIO 14) và triệu hồi Cloud AI chẩn đoán...");
                    return;
                }

                // LỆNH ĐẶC BIỆT 7: Chuyển đổi model Gemini
                if (userText.startsWith("/model ") || userText.startsWith("model "))
                {
                    String newModel = userText.substring(userText.indexOf(' ') + 1);
                    newModel.trim();
                    hybridAI.gemini.setModel(newModel.c_str());
                    Serial.printf("[CLOUD_AI]: Đã chuyển sang model: %s\n", newModel.c_str());
                    return;
                }

                // LỆNH ĐẶC BIỆT 8: Kiểm tra trực tiếp LED RGB: /rgb <r> <g> <b>
                if (userText.startsWith("/rgb ") || userText.startsWith("rgb "))
                {
                    int r = 0, g = 0, b = 0;
                    String params = userText.substring(userText.indexOf(' ') + 1);
                    params.trim();
                    int s1 = params.indexOf(' ');
                    int s2 = (s1 != -1) ? params.indexOf(' ', s1 + 1) : -1;
                    if (s1 != -1 && s2 != -1)
                    {
                        r = params.substring(0, s1).toInt();
                        g = params.substring(s1 + 1, s2).toInt();
                        b = params.substring(s2 + 1).toInt();
                        AIoT_Device.rgb(r, g, b);
                        customLedUntil = millis() + 4000; // Sáng trong 4 giây rồi tự động khôi phục màu trạng thái hệ thống
                        Serial.printf("[HARDWARE_TEST]: Đã xuất tín hiệu RGB (%d, %d, %d) ra chân GPIO %d (Tự về màu hệ thống sau 4s)\n", r, g, b, AIoT_Device.actuators.getRgbPin());
                    }
                    else
                    {
                        Serial.println("Cú pháp: /rgb <r> <g> <b> (Ví dụ: /rgb 255 0 0 bật đỏ, /rgb 0 0 255 xanh lam, /rgb 0 0 0 tắt)");
                    }
                    return;
                }

                // LỆNH ĐẶC BIỆT 9: Kiểm tra nhanh cảm biến biến trở giả lập tại chân GPIO 6: /pot hoặc /adc
                if (userText.equalsIgnoreCase("/pot") || userText.equalsIgnoreCase("pot") ||
                    userText.equalsIgnoreCase("/adc") || userText.equalsIgnoreCase("adc"))
                {
                    potSensor.update();
                    currentPotRaw = potSensor.getRaw();
                    currentPotVolt = potSensor.getVoltage();
                    currentPotPercent = potSensor.getPercent();
                    Serial.println("\n================= POTENTIOMETER SENSOR ================");
                    Serial.printf(" - Chân kết nối ADC : GPIO %d (ADC1_CH5)\n", PIN_POTENTIOMETER);
                    Serial.printf(" - Giá trị ADC Raw  : %d / 4095 (Độ phân giải 12-bit)\n", currentPotRaw);
                    Serial.printf(" - Điện áp đọc được : %.2f V / 3.3V\n", currentPotVolt);
                    Serial.printf(" - Mức tỷ lệ chuẩn  : %.1f %%\n", currentPotPercent);
                    Serial.println("========================================================\n");
                    return;
                }

                // =====================================================================
                // TẦNG 1: KIỂM TRA BỘ QUY TẮC DẠY CÂU LỆNH CỤC BỘ (LOCAL OFFLINE AI)
                // Phản hồi tức thì < 1ms, không cần mạng, không tốn token Cloud!
                // =====================================================================
                String localReply = "";
                if (teachLocalAICommands(userText, localReply))
                {
                    executeAICommands(localReply);
                    Serial.println("--------------------------------------------------");
                    Serial.printf("[LOCAL_EDGE_AI]: %s\n", localReply.c_str());
                    Serial.println("--------------------------------------------------");

                    // Đồng bộ lên MQTT để người dùng trên web/app thấy được phản hồi
                    AIoT.updateTelemetry("user_prompt", userText);
                    AIoT.updateTelemetry("ai_reply", localReply);
                    AIoT.sendTelemetry();
                    return; // Đã xử lý xong cục bộ!
                }

                // =====================================================================
                // TẦNG 2: NẾU LÀ CÂU HỎI MỞ / TRÒ CHUYỆN TỰ DO -> GỬI LÊN GEMINI CLOUD
                // =====================================================================
                float meanVal, rmsVal, p2pVal, stdDevVal;
                hybridAI.edge.extractFeatures(meanVal, rmsVal, p2pVal, stdDevVal);

                potSensor.update();
                currentPotRaw = potSensor.getRaw();
                currentPotVolt = potSensor.getVoltage();
                currentPotPercent = potSensor.getPercent();

                if (hybridAI.gemini.hasApiKey())
                {
                    Serial.println("[CLOUD_AI_HTTPS]: Sending context and query to Gemini...");

                    String promptWithContext =
                        "[Dữ liệu cảm biến phần cứng (chỉ dùng khi người dùng hỏi liên quan): Cảm biến DHT11 (GPIO " + String(PIN_DHT11) + ") Nhiệt độ=" + String(currentTemp, 1) +
                        " °C, Độ ẩm=" + String(currentHum, 1) +
                        " %, Biến trở GPIO 6=" + String(currentPotPercent, 1) +
                        " % (" + String(currentPotVolt, 2) + " V, ADC=" + String(currentPotRaw) +
                        "), Trạng thái Relay 1 (GPIO 14)=" + String(AIoT_Device.getRelay(1) ? "ON" : "OFF") +
                        ", Trạng thái Relay 2 (GPIO 15)=" + String(AIoT_Device.getRelay(2) ? "ON" : "OFF") +
                        ", Edge AI RMS=" + String(rmsVal, 2) +
                        ", Free RAM=" + String(AIoT_Device.readFreeRam()) +
                        " bytes, Chip Temp=" + String(AIoT_Device.readChipTemp(), 1) +
                        " °C]. Câu hỏi người dùng: " + userText;

                    String reply = hybridAI.gemini.ask(promptWithContext, AI_SYSTEM_PROMPT);

                    // Báo lỗi chi tiết và trung thực ra Serial Monitor nếu Cloud AI gặp sự cố
                    if (reply.startsWith("[Error") || reply.indexOf("Error") != -1)
                    {
                        Serial.printf("[GEMINI_ERROR]: %s\n", reply.c_str());
                        if (reply.indexOf("429") != -1)
                        {
                            Serial.println("[NOTE]: Da cham han muc Rate Limit cua Gemini. Vui long doi 30s roi thu lai.");
                        }
                        else if (reply.indexOf("403") != -1)
                        {
                            Serial.println("[NOTE]: Khoa GEMINI_API_KEY bi tu choi. Vui long kiem tra lai Key tren aistudio.google.com.");
                        }
                    }

                    // Phân tích và thực thi các thẻ lệnh phần cứng
                    executeAICommands(reply);

                    Serial.println("--------------------------------------------------");
                    Serial.printf("[AI_ASSISTANT]: %s\n", reply.c_str());
                    Serial.println("--------------------------------------------------");

                    // Đồng bộ lên MQTT
                    AIoT.updateTelemetry("user_prompt", userText);
                    AIoT.updateTelemetry("ai_reply", reply);
                    AIoT.sendTelemetry();
                }
                else
                {
                    Serial.println("[CLOUD_AI_MQTT]: Forwarding prompt to HiveMQ Cloud...");
                    AIoT.updateTelemetry("user_prompt", userText);
                    AIoT.updateTelemetry("dht_temp", currentTemp);
                    AIoT.updateTelemetry("dht_hum", currentHum);
                    AIoT.sendTelemetry();
                }
            }
        }
        // 2. Khi nhấn phím Xóa (Backspace / Delete)
        else if (c == '\b' || (uint8_t)c == 127)
        {
            if (chatInputBuffer.length() > 0)
            {
                while (chatInputBuffer.length() > 0)
                {
                    uint8_t lastByte = (uint8_t)chatInputBuffer.charAt(chatInputBuffer.length() - 1);
                    chatInputBuffer.remove(chatInputBuffer.length() - 1);
                    if ((lastByte & 0xC0) != 0x80)
                        break;
                }
                Serial.print("\b \b");
            }
        }
        // 3. Ký tự thông thường -> Echo ngay lập tức
        else if ((uint8_t)c >= 32 || (uint8_t)c > 127)
        {
            chatInputBuffer += c;
            Serial.print(c);
        }
    }
}

// Đọc tín hiệu cảm biến phục vụ giám sát Edge AI (trả về true khi có mẫu MỚI thực tế)
bool readSensorSignal(float &sampleOut)
{
    if (simulateSensorAnomaly)
    {
        simulateSensorAnomaly = false;
        sampleOut = 85.0f; // Đột biến nhiệt độ cao bất thường
        return true;
    }

    float t, h;
    if (dhtSensor.read(t, h))
    {
        currentTemp = t;
        currentHum = h;
        sampleOut = currentTemp;
        return true;
    }
    else if (!dhtSensor.isOk())
    {
        static unsigned long lastWarn = 0;
        if (millis() - lastWarn > 5000)
        {
            lastWarn = millis();
            Serial.printf("[DHT11_WARN]: Chua doc duoc du lieu tu chan GPIO %d (Kiem tra day cam/tro keo pull-up)\n", PIN_DHT11);
        }
    }
    sampleOut = currentTemp;
    return false;
}

// ======================================================
// SETUP & LOOP
// ======================================================
void setup()
{
    Serial.begin(115200);

    // 1. Khởi tạo thiết bị & cấu hình các chân theo yêu cầu
    AIoT_Device.begin();
    AIoT_Device.Relay(PIN_RELAY1, "Quat chinh");
    AIoT_Device.Relay(PIN_RELAY2, "May bom phu");
    AIoT_Device.setRelayPin(1, PIN_RELAY1); // Chân kích Relay 1 tại GPIO 14
    AIoT_Device.setRelayPin(2, PIN_RELAY2); // Chân kích Relay 2 tại GPIO 15 (Tải 2 / Bơm / Quạt 2)
    AIoT_Device.setRgbPin(PIN_RGB_LED);     // Chân LED RGB onboard tại GPIO 48
    setSystemLed(SYS_LED_BOOT, true);       // Sáng xanh dương nhẹ báo hiệu khởi động
    AIoT_Device.relay(1, false);            // Mặc định tắt Relay 1
    AIoT_Device.relay(2, false);            // Mặc định tắt Relay 2

    // Đọc khởi tạo cảm biến DHT11 lần đầu
    float t, h;
    if (dhtSensor.read(t, h))
    {
        currentTemp = t;
        currentHum = h;
        Serial.printf("[DHT11_INIT]: Khoi tao thanh cong! Temp: %.1f *C, Hum: %.1f %%\n", currentTemp, currentHum);
    }
    else
    {
        Serial.printf("[DHT11_WARN]: Chua nhan duoc tin hieu tu cam bien tai chan GPIO %d khi khoi dong!\n", PIN_DHT11);
    }

    // Khởi tạo cảm biến biến trở giả lập tại chân GPIO 6
    potSensor.begin();
    analogReadResolution(12); // ESP32-S3 ADC 12-bit (0 - 4095)
    Serial.printf("[POTENTIOMETER]: Khoi tao thanh cong tai GPIO %d (ADC 12-bit)\n", PIN_POTENTIOMETER);

    // 2. DẠY CHO EDGE AI: Tự động gom 30 mẫu chu kỳ đầu từ cảm biến để học Baseline
    hybridAI.autoLearn(30, 3.0f);

    // 3. Cấu hình Gemini Cloud AI
    hybridAI.setGeminiApiKey(GEMINI_API_KEY);
    hybridAI.gemini.setModel("gemini-3.5-flash-lite");

    // 4. Khởi động kết nối WiFi & HiveMQ Cloud
    AIoT.begin(WIFI_SSID, WIFI_PASS, MQTT_USER, MQTT_PASS);

    // 5. Đồng bộ Telemetry định kỳ mỗi 5 giây
    AIoT.addTimeEvent(5000, sendHybridTelemetry);

    Serial.println("\n========================================================");
    Serial.println("   HE THONG EDGE AI GIAM SAT & DIEU KHIEN CHUYEN NGHIEP");
    Serial.printf("   - Cam bien DHT11        : GPIO %d\n", PIN_DHT11);
    Serial.printf("   - Cam bien Bien Tro     : GPIO %d (ADC 12-bit)\n", PIN_POTENTIOMETER);
    Serial.printf("   - Ro-le kich tai Relay 1: GPIO %d (Tai chinh / Quat 1)\n", PIN_RELAY1);
    Serial.printf("   - Ro-le kich tai Relay 2: GPIO %d (Tai phu / May bom / Quat 2)\n", PIN_RELAY2);
    Serial.printf("   - Den LED RGB onboard   : GPIO %d\n", PIN_RGB_LED);
    Serial.println("   - Co che giam sat       : Edge AI (Baseline + Z-Score + Dong hoc cua so truot)");
    Serial.println("--------------------------------------------------------");
    Serial.println("CAC LENH DIEU KHIEN & TRUY VAN TREN SERIAL MONITOR:");
    Serial.println("    - Dieu khien Relay 1: 'bat relay 1', 'tat relay 1', 'bat quat 1'");
    Serial.println("    - Dieu khien Relay 2: 'bat relay 2', 'tat relay 2', 'bat may bom'");
    Serial.println("    - Tra cuu du lieu   : 'nhiet do phong?', 'do am?', 'bien tro?', 'kiem tra relay'");
    Serial.println("    - Doi mau den LED   : 'led xanh la', 'led do', 'led xanh duong', 'bip coi'");
    Serial.println("    - Lenh he thong     : '/edge', '/cloud', '/pot', '/teach', '/autolearn'");
    Serial.println("========================================================\n");
}

void loop()
{
    // 1. Duy trì kết nối IoT & MQTT
    AIoT.run();

    // 1.1. Tự động khôi phục màu LED trạng thái hoạt động sau khi lệnh tạm thời kết thúc
    if (customLedUntil > 0 && millis() >= customLedUntil)
    {
        restoreSystemLed();
    }

    // Cập nhật giá trị cảm biến biến trở tại GPIO 6 liên tục
    potSensor.update();
    currentPotRaw = potSensor.getRaw();
    currentPotVolt = potSensor.getVoltage();
    currentPotPercent = potSensor.getPercent();

    // 2. TẦNG 1: Edge AI chỉ tiếp nhận và tính toán khi có mẫu đo MỚI từ cảm biến (chu kỳ 2s)
    float sensorValue = currentTemp;
    static bool wasEmergency = false;
    static bool calibrationDoneNotified = false;

    if (readSensorSignal(sensorValue))
    {
        // 1. Thực thi suy luận Edge AI tại chỗ (Z-Score, Động học P2P, Sốc nhiệt & Lọc nhiễu)
        EdgeAI::InferenceResult res = hybridAI.process(sensorValue, PIN_RELAY1);

        // 2. PHẢN XẠ VÀ PHÂN CẤP CHỈ BÁO THEO 3 CẤP ĐỘ EDGE AI:
        // CẤP 3: NGUY HIỂM KHẨN CẤP (ĐÈN ĐỎ)
        if (res.state == EdgeAI::STATE_CRITICAL)
        {
            wasEmergency = true;
            AIoT_Device.relay(1, false);    // Ngắt ngay Relay 1 để bảo vệ tải chính
            setSystemLed(SYS_LED_CRITICAL); // Đèn ĐỎ rực cảnh báo khẩn cấp

            static unsigned long lastIncidentAlert = 0;
            if (millis() - lastIncidentAlert > 30000)
            {
                lastIncidentAlert = millis();
                Serial.println("\n========================================================");
                Serial.printf("[EDGE_AI_ALERT]: Val = %.1f *C | Z = %.2f sigma (%s)\n",
                              sensorValue, res.zScore, res.reason);
                Serial.printf("[DYNAMIC_ANALYSIS]: Baseline = %.2f *C | P2P = %.2f *C | StdDev = %.2f\n",
                              hybridAI.edge.getDetector().getBaselineMean(), res.p2p, res.stdDev);
                Serial.println("[FAILSAFE_ACTION]: Da tu dong ngat Relay 1 (GPIO 14) de bao ve!");
                AIoT_Device.beep(200);

                // Tự động kích hoạt Cloud AI để chẩn đoán nguyên nhân sự cố
                if (hybridAI.gemini.hasApiKey() && WiFi.status() == WL_CONNECTED)
                {
                    Serial.println("[EDGE_TO_CLOUD_AI]: Trieu hoi Gemini LLM phan tich su co...");

                    String incidentPrompt =
                        "HỆ THỐNG PHÁT HIỆN SỰ CỐ NHIỆT ĐỘ DỊ THƯỜNG THUẦN TÚY BẰNG THUẬT TOÁN EDGE AI!\n"
                        "- Nhiệt độ cảm biến DHT11 (GPIO " +
                        String(PIN_DHT11) + "): " + String(currentTemp, 1) + " °C\n"
                                                                             "- Độ ẩm hiện tại: " +
                        String(currentHum, 1) + " %\n"
                                                "- Biến trở GPIO 6: " +
                        String(currentPotPercent, 1) + " % (" + String(currentPotVolt, 2) + " V)\n"
                                                                                            "- Edge AI Z-Score: " +
                        String(res.zScore, 2) + " sigma (Ngưỡng " + String(hybridAI.edge.getDetector().getThreshold(), 1) + "σ)\n"
                                                                                                                            "- Anomaly Score: " +
                        String(res.score, 2) + " / 1.00\n"
                                               "- Chẩn đoán Edge AI: " +
                        String(res.reason) + "\n"
                                             "- Baseline Mean: " +
                        String(hybridAI.edge.getDetector().getBaselineMean(), 2) + " °C\n"
                                                                                   "- Cửa sổ trượt 64 mẫu: RMS = " +
                        String(res.rms, 2) + ", Peak-to-Peak = " + String(res.p2p, 2) + " °C, StdDev = " + String(res.stdDev, 2) + "\n"
                                                                                                                                   "- Mạng nơ-ron TinyML (ESP32-S3): Lớp = " +
                        String(EdgeAI::stateToString(res.neural.predictedClass)) + ", Độ tin cậy = " + String(res.neural.confidence * 100.0f, 1) + "% (Độ trễ: " + String(res.neural.inferenceTimeUs) + " µs)\n"
                                                                                                                                                                                                        "- Relay 1: " +
                        String(AIoT_Device.getRelay(1) ? "ON" : "OFF") + ", Relay 2: " + String(AIoT_Device.getRelay(2) ? "ON" : "OFF") + "\n"
                                                                                                                                          "Edge AI đã tự động ngắt Relay 1 (GPIO 14). Bạn hãy phân tích ngắn gọn nguyên nhân khả dĩ và hướng dẫn xử lý.";

                    String cloudDiagnosis = hybridAI.gemini.ask(incidentPrompt, AI_SYSTEM_PROMPT);
                    executeAICommands(cloudDiagnosis);

                    Serial.println("----------------------------------------------------------");
                    Serial.printf("[CLOUD_AI_DIAGNOSIS]:\n%s\n", cloudDiagnosis.c_str());
                    Serial.println("==========================================================\n");

                    AIoT.updateTelemetry("incident_score", res.score);
                    AIoT.updateTelemetry("tinyml_class", EdgeAI::stateToString(res.neural.predictedClass));
                    AIoT.updateTelemetry("tinyml_confidence", res.neural.confidence);
                    AIoT.updateTelemetry("tinyml_latency_us", (int)res.neural.inferenceTimeUs);
                    AIoT.updateTelemetry("incident_diagnosis", cloudDiagnosis);
                    AIoT.sendTelemetry();
                }
            }
        }
        // CẤP 2: BẤT THƯỜNG / CẢNH BÁO SỚM (ĐÈN TRẮNG)
        else if (res.state == EdgeAI::STATE_WARNING)
        {
            if (!wasEmergency)
            {
                setSystemLed(SYS_LED_WARNING);
            }
        }
        // CẤP 1: BÌNH THƯỜNG / AN TOÀN (ĐÈN XANH LÁ)
        else
        {
            if (wasEmergency)
            {
                wasEmergency = false;
                Serial.println("[SYSTEM_RECOVERY]: Du lieu da tro lai phan phoi an toan cua Edge AI.");
            }

            if (hybridAI.edge.getDetector().isCalibrated())
            {
                if (!calibrationDoneNotified)
                {
                    calibrationDoneNotified = true;
                    Serial.println("[EDGE_AI]: Da hoan thanh can chinh baseline. He thong san sang giam sat.");
                }
                setSystemLed(SYS_LED_NORMAL);
            }
        }
    }

    // 3. In trực tiếp nhiệt độ, độ ẩm, biến trở & trạng thái 2 Relay ra Serial Monitor (chu kỳ 2 giây)
    static unsigned long lastMonitorPrint = 0;
    if (millis() - lastMonitorPrint >= 2000)
    {
        lastMonitorPrint = millis();
        if (chatInputBuffer.length() == 0)
        {
            float zNow = hybridAI.edge.getDetector().getZScore(currentTemp);
            float scoreNow = hybridAI.edge.getDetector().predictScore(currentTemp);
            const char *stateStr = (zNow >= 3.0f) ? "CRITICAL" : ((zNow >= 1.5f) ? "WARNING" : "NORMAL");

            if (dhtSensor.isOk())
            {
                Serial.printf("[DHT11]: %.1f *C, %.1f %% | [Edge AI]: Z=%.2f sigma [%s - Score: %.2f] | [POT]: %.1f %% | R1: %s | R2: %s\n",
                              currentTemp, currentHum, zNow, stateStr, scoreNow,
                              currentPotPercent,
                              AIoT_Device.getRelay(1) ? "ON" : "OFF",
                              AIoT_Device.getRelay(2) ? "ON" : "OFF");
            }
            else
            {
                Serial.printf("[DHT11]: Reading... | [POT]: %.1f %% | R1: %s | R2: %s\n",
                              currentPotPercent,
                              AIoT_Device.getRelay(1) ? "ON" : "OFF",
                              AIoT_Device.getRelay(2) ? "ON" : "OFF");
            }
        }
    }

    // 4. Xử lý giao tiếp Terminal & Chat thông minh hai chiều
    handleSerialChat(sensorValue);

    delay(50);
}