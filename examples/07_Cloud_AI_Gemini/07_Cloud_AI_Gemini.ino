/**
 * ============================================================================
 * AIoT_LIB - Ví Dụ 07: Trí Tuệ Đám Mây Google Gemini (Cloud AI & Action Dispatch)
 * ============================================================================
 * Chức năng:
 * - Gửi yêu cầu và dữ liệu ngữ cảnh lên Google Gemini qua HTTPS REST API
 * - Sử dụng Hardware Prompt tự sinh của AIoT_Device làm System Instruction
 * - Cơ chế Function Calling an toàn: Gemini phản hồi văn bản kèm thẻ [CMD:...]
 * - Bo mạch tự động phân giải và thực thi phần cứng qua AIoT_Device.executeCommand()
 * ============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <AIoT.h>
#include <CloudAI/CloudAI.h>

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASS";

// Khóa API Google Gemini (Lấy miễn phí tại: https://aistudio.google.com)
const char *GEMINI_API_KEY = "YOUR_GEMINI_API_KEY";

CloudAI::GeminiClient gemini(GEMINI_API_KEY, "gemini-2.5-flash");

#define PIN_FAN_RELAY 14 // Rơ-le quạt làm mát
#define PIN_BUZZER    4  // Còi cảnh báo

void askGeminiForAction(const String &question)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[ERROR] Chua co ket noi WiFi.");
        return;
    }

    Serial.println("\n[CLOUD_AI] Dang gui yeu cau len Google Gemini: " + question);

    // 1. Lấy mô tả cấu hình phần cứng tự sinh làm System Prompt
    String systemPrompt = AIoT_Device.getHardwarePrompt();
    systemPrompt += "Quy tac: Khi nhiet do cao hay bat quat bang tag [CMD:RELAY:" + String(PIN_FAN_RELAY) + ":ON].\n";

    // 2. Gọi Gemini API qua HTTPS
    String aiReply = gemini.ask(question, systemPrompt);

    Serial.println("\n--- Phản hồi từ Gemini ---");
    Serial.println(aiReply);
    Serial.println("---------------------------");

    // 3. Tự động bóc tách và thực thi tag lệnh có trong câu trả lời
    AIoT_Device.executeCommand(aiReply);
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== AIoT_LIB: Vi Du 07 - Google Gemini Cloud AI ===");

    // Đăng ký thiết bị phần cứng
    AIoT_Device.Relay(PIN_FAN_RELAY, "Quạt giải nhiệt");
    AIoT_Device.setBuzzerPin(PIN_BUZZER);

    // Kết nối WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Dang ket noi WiFi...");
    while (WiFi.status() != WL_CONNECTED && millis() < 10000)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("WiFi da ket noi! IP: %s\n", WiFi.localIP().toString().c_str());

        // Thử nghiệm gửi câu hỏi phân tích tình huống cho Gemini
        askGeminiForAction("Nhiet do may dang la 82 do C, rat nong! Ban hay dua ra chi thi.");
    }
    else
    {
        Serial.println("Chua ket noi duoc WiFi. Vui long kiem tra lai SSID/PASS.");
    }
}

void loop()
{
    delay(1000);
}

