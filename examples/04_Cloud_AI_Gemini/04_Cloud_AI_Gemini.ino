/**
 * ============================================================================
 * AIoT_LIB - Ví Dụ 04: Trí Tuệ Đám Mây Google Gemini & Action Calling
 * ============================================================================
 * Mô tả:
 * - Tự động tạo System Prompt phần cứng từ cấu hình thực tế của AIoT_Device
 * - Gửi câu hỏi và dữ liệu cảm biến trực tiếp lên Google Gemini qua HTTPS TLS
 * - Nhận phản hồi từ Gemini và tự động bóc tách lệnh [CMD:RELAY:<pin>:ON|OFF]
 * - Thực thi điều khiển Relay / Còi / Đèn trực tiếp mà không cần code parser
 */

#include <Arduino.h>
#include <WiFi.h>
#include <AIoT.h>
#include <CloudAI/CloudAI.h>

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASS";

// Khóa API Google Gemini (Lấy miễn phí tại: https://aistudio.google.com)
const char *GEMINI_API_KEY = "YOUR_GEMINI_API_KEY";

// Khởi tạo Gemini Client (Model mặc định: gemini-2.5-flash hoặc gemini-1.5-flash)
CloudAI::GeminiClient gemini(GEMINI_API_KEY, "gemini-2.5-flash");

#define PIN_FAN_RELAY  14 // Rơ-le quạt làm mát
#define PIN_PUMP_RELAY 15 // Rơ-le máy bơm
#define PIN_BUZZER     4  // Còi cảnh báo

void sendQueryToGemini(const String &situationDescription)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[ERROR] Chua co ket noi WiFi de goi Gemini API.");
        return;
    }

    if (!gemini.hasApiKey() || strcmp(GEMINI_API_KEY, "YOUR_GEMINI_API_KEY") == 0)
    {
        Serial.println("[WARN] Vui long dien GEMINI_API_KEY hop le de test goi Cloud.");
        return;
    }

    Serial.println("\n--- [DANG GUI YEU CAU LEN GOOGLE GEMINI] ---");
    Serial.println("Noi dung gui: " + situationDescription);

    // 1. Lấy thông tin cấu hình phần cứng tự động làm System Instruction
    String systemPrompt = AIoT_Device.getHardwarePrompt();
    systemPrompt += "Quy tac: Neu nhiet do cao bat quat [CMD:RELAY:" + String(PIN_FAN_RELAY) + ":ON].\n";

    // 2. Gọi API Gemini qua giao thức HTTPS
    String aiResponse = gemini.ask(situationDescription, systemPrompt);

    Serial.println("\n--- [PHAN HOI TU GEMINI] ---");
    Serial.println(aiResponse);
    Serial.println("----------------------------");

    // 3. Tự động bóc tách và thực thi các tag lệnh có trong phản hồi
    bool hasExecuted = AIoT_Device.executeCommand(aiResponse);
    if (hasExecuted)
    {
        Serial.println("[SYSTEM] Da tu dong thuc thi lenh phan cung tu phan hoi cua Gemini!");
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=======================================================");
    Serial.println("   AIoT_LIB: Google Gemini Cloud AI & Action Dispatcher");
    Serial.println("=======================================================");

    // 1. Đăng ký chân thiết bị động
    AIoT_Device.Relay(PIN_FAN_RELAY, "Quat lam mat tu dong");
    AIoT_Device.Relay(PIN_PUMP_RELAY, "May bom ap luc");
    AIoT_Device.setBuzzerPin(PIN_BUZZER);

    // In cấu hình phần cứng mẫu
    Serial.println("\n[HARDWARE CONFIG]");
    Serial.println(AIoT_Device.getHardwarePrompt());

    // 2. Kết nối WiFi
    Serial.printf("[WIFI] Dang ket noi toi: %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    // Đợi kết nối tối đa 10 giây
    int timeout = 20;
    while (WiFi.status() != WL_CONNECTED && timeout > 0)
    {
        delay(500);
        Serial.print(".");
        timeout--;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("[WIFI] Da ket noi! IP: %s\n", WiFi.localIP().toString().c_str());

        // Mô phỏng gửi tình huống cho Gemini chẩn đoán và điều khiển
        String prompt = "Nhiet do dong co dang la 78.5 do C (vuot nguong 65 do C). Ban hay dua ra chi thi dieu khien.";
        sendQueryToGemini(prompt);
    }
    else
    {
        Serial.println("[WIFI] Khong ket noi duoc WiFi. Minh hoa cach xu ly chuoi lenh offline:");
        // Mô phỏng chuỗi lệnh giả lập nhận từ AI để test bộ thực thi lệnh
        String mockAiReply = "Nhiet do qua cao! He thong can bat quat lam mat: [CMD:RELAY:" + String(PIN_FAN_RELAY) + ":ON] va phat coi bao [CMD:BEEP]";
        AIoT_Device.executeCommand(mockAiReply);
    }
}

void loop()
{
    delay(1000);
}
