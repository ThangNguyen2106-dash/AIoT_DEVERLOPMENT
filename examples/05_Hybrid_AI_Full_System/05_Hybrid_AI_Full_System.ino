/**
 * ==============================================================================
 * VÍ DỤ 04: HYBRID AI FULL SYSTEM (HỆ THỐNG ĐIỆN TOÁN LAI HOÀN CHỈNH)
 * ==============================================================================
 * Minh họa sự cộng tác toàn diện giữa Edge AI và Cloud AI:
 * 1. Edge AI chạy suy luận độc lập tại biên (<0.1ms) bảo vệ thiết bị.
 * 2. HybridAI đóng gói toàn bộ 16 đặc trưng + trạng thái gửi lên Cloud/MQTT.
 * 3. Tình huống người dùng tự quyết:
 *    - Khi độ tự tin của Edge AI thấp (< 65%) hoặc phát hiện rung lạ:
 *      -> Tự động gọi consultCloud() gửi 16 đặc trưng lên Gemini xin chẩn đoán chuyên sâu!
 * 4. Nhận bản tin JSON từ Cloud/Web -> Gọi syncModelFromJson() cập nhật NVS Flash ngay lập tức!
 * ==============================================================================
 */

#define DEBUG_COLOR
#include <Arduino.h>
#include <AIoT.h>

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";
const char *GEMINI_API_KEY = "YOUR_GEMINI_API_KEY_HERE";

// 1. Trọng số mặc định xuất xưởng (3 Labels + 2 Commands x 16 Inputs)
const char *LABEL_NAMES[3] = {"NORMAL", "WARNING", "CRITICAL_FAULT"};
const float W[80] = {
    -0.5f, -0.6f, -0.4f, -0.3f, -0.4f, -0.5f, -0.3f, -0.2f, -0.3f, -0.4f, -0.2f, -0.1f, -0.3f, -0.3f, -0.2f, -0.1f,
    0.4f, 0.5f, 0.3f, 0.2f, 0.3f, 0.4f, 0.2f, 0.1f, 0.6f, 0.7f, 0.3f, 0.2f, 0.4f, 0.5f, 0.3f, 0.2f,
    0.9f, 1.0f, 0.8f, 0.7f, 0.9f, 0.9f, 0.7f, 0.6f, 0.6f, 0.7f, 0.5f, 0.4f, 0.8f, 0.9f, 0.7f, 0.6f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.4f, 0.2f, 0.1f, 0.9f, 0.8f, 0.5f, 0.3f, 0.1f, 0.1f, 0.0f, 0.0f,
    0.8f, 0.9f, 0.9f, 0.6f, 0.5f, 0.6f, 0.4f, 0.3f, 0.3f, 0.4f, 0.2f, 0.1f, 0.8f, 0.8f, 0.7f, 0.5f};
const float b[5] = {0.5f, -0.2f, -1.0f, -0.6f, -1.5f};

const float meanVals[16] = {25.0f, 26.0f, 5.0f, 1.2f, 10.0f, 10.5f, 2.0f, 0.5f, 45.0f, 45.2f, 3.0f, 0.8f, 50.0f, 51.0f, 6.0f, 1.5f};
const float stdDevVals[16] = {4.5f, 4.8f, 1.5f, 0.4f, 2.0f, 2.1f, 0.8f, 0.2f, 5.0f, 5.1f, 1.2f, 0.3f, 8.0f, 8.2f, 2.0f, 0.5f};

void setup()
{
    Serial.begin(115200);

    // [A] Khởi tạo thiết bị phần cứng
    AIoT.device.begin();
    AIoT.edgeAI.begin(16, 4); // 4 kênh, cửa sổ 16 mẫu

    // [B] Cơ chế nạp 2 tầng (NVS Flash vs Factory Default)
    if (AIoT.edgeAI.loadFromNVS())
    {
        Serial.printf("[SYSTEM] Đã nạp mô hình thích nghi từ NVS Flash! (Norm: %s)\n",
                      AIoT.edgeAI.getNormTypeName());
    }
    else
    {
        Serial.println(F("[SYSTEM] NVS trống -> Nạp mô hình gốc (Factory Default):"));
        AIoT.edgeAI.setModel(W, b, 16, 3, 2);
        AIoT.edgeAI.setZScore(meanVals, stdDevVals);
    }

    // [C] Khởi tạo Cloud AI (Gemini)
    AIoT.cloudAI.begin(GEMINI_API_KEY, "gemini-1.5-flash");

    // [D] Kết nối WiFi (nếu có thông tin)
    if (strlen(WIFI_SSID) > 0 && strcmp(WIFI_SSID, "YOUR_WIFI_SSID") != 0)
    {
        WiFi.begin(WIFI_SSID, WIFI_PASS);
    }

    Serial.println(F("\n[HYBRID AI] Hệ thống Hybrid AI đã sẵn sàng vận hành!"));
}

void loop()
{
    AIoT.run();

    // 1. Đọc và đẩy 4 cảm biến vào Edge AI
    AIoT.edgeAI.push(0, (float)analogRead(32)); // Rung
    AIoT.edgeAI.push(1, (float)analogRead(33)); // Dòng
    AIoT.edgeAI.push(2, (float)analogRead(34)); // Nhiệt
    AIoT.edgeAI.push(3, (float)analogRead(35)); // Tiếng ồn

    // 2. Khi đủ 16 mẫu trượt
    if (AIoT.edgeAI.isReady())
    {
        // Thực thi suy luận Edge AI (< 0.1ms)
        size_t label = AIoT.edgeAI.predict();
        float conf = AIoT.edgeAI.getWinnerConfidence();

        Serial.printf("[EDGE] State: %s (%.1f%%) | Exec: %lu us\n",
                      LABEL_NAMES[label], conf * 100.0f,
                      (unsigned long)AIoT.edgeAI.getExecutionTime());

        // =====================================================================
        // TÌNH HUỐNG HYBRID AI (DO NGƯỜI DÙNG QUYẾT ĐỊNH)
        // =====================================================================
        // Tình huống: Khi độ tự tin của Edge AI rớt xuống dưới 65% (máy có dấu hiệu lạ)
        // -> Tự động leo thang nhờ Gemini Cloud chẩn đoán sâu!
        static unsigned long lastCloudAsk = 0;
        if (conf < 0.65f && millis() - lastCloudAsk > 30000) // Giãn cách 30s
        {
            lastCloudAsk = millis();
            Serial.println(F("\n[HYBRID AI] >> CẢNH BÁO: Độ tự tin Edge AI thấp (<65%)!"));
            Serial.println(F("[HYBRID AI] >> Đang gửi 16 đặc trưng lên Cloud Gemini xin chẩn đoán..."));

            String diag = AIoT.hybridAI.consultCloud("Độ tự tin tại biên bị suy giảm, hãy phân tích nguyên nhân.");
            Serial.println(F("[GEMINI CHẨN ĐOÁN]:"));
            Serial.println(diag);
            Serial.println(F("-----------------------------------------------------------------"));
        }

        // =====================================================================
        // ĐÓNG GÓI TELEMETRY GỬI CLOUD / MQTT
        // =====================================================================
        static unsigned long lastTele = 0;
        if (millis() - lastTele > 5000) // Mỗi 5s
        {
            lastTele = millis();
            String teleJson = AIoT.hybridAI.serializeTelemetry("ESP32_DEMO_MAC");
            Serial.printf("[UPLINK JSON] %s\n", teleJson.c_str());
        }
    }

    // =========================================================================
    // DEMO NHẬN BẢN TIN ĐỒNG BỘ MÔ HÌNH (DOWNLINK MODEL SYNC TỪ CLOUD / WEB)
    // =========================================================================
    if (Serial.available())
    {
        char c = Serial.read();
        if (c == 'm') // Giả lập nhận bản tin JSON cập nhật trọng số từ Cloud
        {
            Serial.println(F("\n[HYBRID AI] >> Nhận bản tin JSON cập nhật trọng số từ Cloud..."));

            // Chuỗi JSON mẫu nhận từ MQTT / Web (cập nhật lại trọng số và Z-Score)
            const char *sampleJson =
                "{\"input_dim\":16,\"num_labels\":3,\"num_cmds\":2,\"norm_type\":2,"
                "\"W\":[-0.4,-0.5,-0.3,-0.2,-0.3,-0.4,-0.2,-0.1,-0.2,-0.3,-0.1,0.0,-0.2,-0.2,-0.1,0.0,"
                "0.5,0.6,0.4,0.3,0.4,0.5,0.3,0.2,0.7,0.8,0.4,0.3,0.5,0.6,0.4,0.3,"
                "0.9,1.0,0.8,0.7,0.9,0.9,0.7,0.6,0.6,0.7,0.5,0.4,0.8,0.9,0.7,0.6,"
                "0.0,0.0,0.0,0.0,0.3,0.4,0.2,0.1,0.9,0.8,0.5,0.3,0.1,0.1,0.0,0.0,"
                "0.8,0.9,0.9,0.6,0.5,0.6,0.4,0.3,0.3,0.4,0.2,0.1,0.8,0.8,0.7,0.5],"
                "\"b\":[0.6,-0.1,-1.0,-0.5,-1.4],"
                "\"norm1\":[26.0,27.0,5.2,1.3,10.2,10.8,2.1,0.6,46.0,46.5,3.1,0.9,51.0,52.0,6.2,1.6],"
                "\"norm2\":[4.6,4.9,1.6,0.4,2.1,2.2,0.9,0.2,5.1,5.2,1.3,0.3,8.1,8.3,2.1,0.5]}";

            if (AIoT.hybridAI.syncModelFromJson(sampleJson))
            {
                Serial.println(F("[HYBRID AI] >> Đồng bộ và Hot-reload mô hình vào Flash NVS THÀNH CÔNG!"));
            }
        }
    }

    delay(20);
}
