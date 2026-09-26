/**
 * ==============================================================================
 * VÍ DỤ 03: EDGE AI ADAPTIVE NVS (LƯU TRỮ VÀ THÍCH NGHI QUA NVS FLASH)
 * ==============================================================================
 * Hướng dẫn quản lý vòng đời mô hình thích nghi trên ESP32:
 * 1. Khởi động 2 tầng (2-Tier Fallback):
 *    - Tầng 1: Thử nạp mô hình thích nghi mới nhất đã lưu trong NVS Flash.
 *    - Tầng 2: Nếu Flash trống -> Tự động dùng mô hình mặc định xuất xưởng.
 * 2. Hỗ trợ lệnh tương tác qua Serial Monitor:
 *    - Gõ 's' -> Lưu mô hình hiện tại xuống NVS Flash (kèm kiểm tra CRC32).
 *    - Gõ 'c' -> Xóa NVS Flash (Khôi phục cài đặt gốc / Factory Reset).
 *    - Gõ 'l' -> Nạp lại mô hình từ Flash NVS.
 *    - Gõ 'p' -> In bảng tóm tắt chẩn đoán chi tiết (Inference Summary).
 * ==============================================================================
 */

#define DEBUG_COLOR
#include <Arduino.h>
#include <AIoT.h>

const float W_DEFAULT[8] = {
    // 2 Labels x 4 Inputs = 8 Trọng số mặc định
    -0.5f, -0.6f, -0.4f, -0.3f, // Label 0: NORMAL
    0.8f, 0.9f, 0.7f, 0.6f      // Label 1: FAULT
};
const float b_DEFAULT[2] = {0.5f, -0.8f};

const float mean_DEFAULT[4] = {25.0f, 26.0f, 5.0f, 1.2f};
const float std_DEFAULT[4] = {4.5f, 4.8f, 1.5f, 0.4f};

void setup()
{
    Serial.begin(115200);

    AIoT.device.begin();
    AIoT.edgeAI.begin(16, 1); // 1 kênh cảm biến, cửa sổ 16 mẫu

    // =========================================================================
    // CƠ CHẾ KHỞI ĐỘNG 2 TẦNG (FALLBACK MECHANISM)
    // =========================================================================
    if (AIoT.edgeAI.loadFromNVS())
    {
        Serial.printf("[EDGE_AI] Đã nạp thành công mô hình thích nghi từ NVS Flash! (Norm: %s)\n",
                      AIoT.edgeAI.getNormTypeName());
    }
    else
    {
        Serial.println(F("[EDGE_AI] NVS Flash trống -> Dùng mô hình mặc định xuất xưởng (Factory Default):"));
        AIoT.edgeAI.setModel(W_DEFAULT, b_DEFAULT, 4, 2, 0);
        AIoT.edgeAI.setZScore(mean_DEFAULT, std_DEFAULT);
    }

    Serial.println(F("\n--- HƯỚNG DẪN LỆNH SERIAL ---"));
    Serial.println(F("  's' : Lưu mô hình hiện tại vào NVS Flash"));
    Serial.println(F("  'c' : Xóa trắng NVS Flash (Factory Reset)"));
    Serial.println(F("  'l' : Nạp lại mô hình từ NVS Flash"));
    Serial.println(F("  'p' : In tóm tắt kết quả suy luận"));
    Serial.println(F("------------------------------\n"));
}

void loop()
{
    AIoT.run();

    // Giả lập đọc cảm biến rung động
    float sample = 25.0f + (float)random(-5, 5);
    AIoT.edgeAI.push(sample);

    if (AIoT.edgeAI.isReady())
    {
        AIoT.edgeAI.predict();
    }

    // Xử lý lệnh tương tác người dùng qua Serial
    if (Serial.available())
    {
        char cmd = Serial.read();
        switch (cmd)
        {
        case 's': // Lưu mô hình hiện tại xuống Flash
            if (AIoT.edgeAI.saveCurrentToNVS())
            {
                Serial.println(F("[NVS] >> Đã lưu thành công mô hình vào NVS Flash!"));
            }
            break;

        case 'c': // Xóa trắng NVS (Factory Reset)
            if (AIoT.edgeAI.clearNVS())
            {
                Serial.println(F("[NVS] >> Đã xóa trắng Flash! Vui lòng khởi động lại để kiểm tra fallback."));
            }
            break;

        case 'l': // Nạp từ Flash
            if (AIoT.edgeAI.loadFromNVS())
            {
                Serial.println(F("[NVS] >> Đã nạp lại mô hình từ Flash!"));
            }
            else
            {
                Serial.println(F("[NVS] >> Không tìm thấy dữ liệu hợp lệ trong Flash!"));
            }
            break;

        case 'p': // In tóm tắt
            AIoT.edgeAI.printSummary();
            break;
        }
    }

    delay(20);
}
