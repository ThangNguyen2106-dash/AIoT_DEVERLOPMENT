/**
 * ============================================================================
 * AIoT_LIB - Ví Dụ 09: Bộ Điều Phối Thông Minh (Hybrid AI Orchestrator)
 * ============================================================================
 * Chức năng:
 * - Điều phối luồng xử lý thông minh dựa trên chính sách PolicyEngine (Edge-First, Cloud-Assist)
 * - Tối ưu hóa chi phí API và băng thông: Chỉ gửi lên Cloud khi cần thiết
 * ============================================================================
 */

#include <Arduino.h>
#include <HybridAI/HybridAI.hpp>

HybridAI::Engine hybridAI;

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== AIoT_LIB: Vi Du 09 - Hybrid AI Policy Orchestrator ===");

    // Tạo gói dữ liệu đầu vào mô phỏng
    HybridAI::AIInput input;
    input.value = 0.82f;
    input.confidence = 0.91f;
    input.severity = HybridAI::Severity::WARNING;
    input.realtime = true;

    // Bộ điều phối tự động quyết định luồng xử lý: Edge hay Cloud
    HybridAI::AIResult result = hybridAI.process(input);

    Serial.printf("- Huong xu ly (Path): %s\n", result.label ? result.label : "NONE");
    Serial.printf("- Quyet dinh (Decision): %d\n", static_cast<int>(result.decision));
    Serial.printf("- Ly do dieu phoi (Reason): %s\n", result.reason ? result.reason : "NONE");
}

void loop()
{
    delay(10000);
}
