/**
 * ==============================================================================
 * VÍ DỤ 04: CLOUD AI GEMINI (KẾT NỐI TRỰC TIẾP GOOGLE GEMINI HTTPS)
 * ==============================================================================
 * Hướng dẫn sử dụng module Cloud AI gọi trực tiếp Google Gemini qua HTTPS:
 * 1. Kết nối WiFi
 * 2. Cấu hình GEMINI_API_KEY và Model (mặc định: gemini-1.5-flash)
 * 3. Gửi prompt hỏi đáp qua Serial Monitor và nhận câu trả lời dạng văn bản
 * ==============================================================================
 */

#define DEBUG_COLOR
#include <Arduino.h>
#include <AIoT.h>

// Cấu hình mạng WiFi
const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";

// Khóa API Google Gemini (Lấy miễn phí tại: https://aistudio.google.com)
const char *GEMINI_API_KEY = "YOUR_GEMINI_API_KEY_HERE";

void setup()
{
    Serial.begin(115200);

    // 1. Kết nối WiFi
    Serial.printf("[WIFI] Đang kết nối tới %s...", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n[WIFI] Đã kết nối! IP: %s\n", WiFi.localIP().toString().c_str());

    // 2. Khởi tạo Cloud AI Gemini
    AIoT.cloudAI.begin(GEMINI_API_KEY, "gemini-1.5-flash");

    Serial.println(F("\n--- THỬ NGHIỆM CLOUD AI GEMINI ---"));
    Serial.println(F("Gửi câu hỏi mẫu: 'Giải thích nguyên lý bảo trì dự đoán trong IoT'"));

    // 3. Gửi câu hỏi lên Gemini
    String reply = AIoT.cloudAI.ask(
        "Giải thích ngắn gọn 3 dòng về lợi ích của Edge AI kết hợp Cloud AI trong công nghiệp.",
        "Bạn là chuyên gia kỹ thuật AIoT. Hãy trả lời ngắn gọn, chuyên nghiệp bằng tiếng Việt.");

    Serial.println(F("\n[GEMINI PHẢN HỒI]:"));
    Serial.println(reply);

    Serial.println(F("\n>> Hãy gõ câu hỏi bất kỳ vào Serial Monitor để trò chuyện với Gemini..."));
}

void loop()
{
    AIoT.run();

    // Nhận câu hỏi từ Serial Monitor
    if (Serial.available())
    {
        String prompt = Serial.readStringUntil('\n');
        prompt.trim();

        if (prompt.length() > 0)
        {
            Serial.printf("\n[USER]: %s\n", prompt.c_str());
            Serial.println(F("[GEMINI]: Đang xử lý..."));

            String answer = AIoT.cloudAI.ask(prompt);

            Serial.println(F("[GEMINI TRẢ LỜI]:"));
            Serial.println(answer);
            Serial.println(F("---------------------------------------------"));
        }
    }

    delay(20);
}
