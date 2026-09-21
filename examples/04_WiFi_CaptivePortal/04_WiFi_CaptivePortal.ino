/**
 * ============================================================================
 * AIoT_LIB - Ví Dụ 04: Cấu Hình Mạng Không Dây (Smart Captive Portal)
 * ============================================================================
 * Chức năng:
 * - Khi chưa lưu WiFi trong Flash, bo mạch tự động phát mạng Access Point: "AIoT_WiFi"
 * - Người dùng kết nối vào Access Point, mở trình duyệt điện thoại tại: 192.168.21.6
 * - Giao diện Web Responsive hỗ trợ quét sóng WiFi xung quanh và lưu cấu hình vĩnh viễn
 * - Hoàn toàn không cần cắm cáp nạp lại code khi mang thiết bị sang môi trường mạng mới
 * ============================================================================
 */

#include <Arduino.h>

#ifndef DEBUG_COLOR
#define DEBUG_COLOR
#endif

#ifndef BUTTON_CONFIG
#define BUTTON_CONFIG // Bật chế độ cấu hình Captive Portal AP
#endif

#include <AIoT.h>

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== AIoT_LIB: Vi Du 04 - Smart Captive Portal ===");
    Serial.println("Neu chua co WiFi luu trong Flash, bo mach se phat Access Point: AIoT_WiFi");
    Serial.println("Dia chi IP cau hinh Web: 192.168.21.6");

    // Khởi động chế độ Captive Portal (để trống SSID và Pass)
    AIoT.begin("", "");
}

void loop()
{
    // Duy trì Web Server cấu hình và dịch vụ DNS Captive Portal
    AIoT.run();
}
