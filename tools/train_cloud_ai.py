"""
=============================================================================
           AIoT_LIB - HƯỚNG DẪN DẠY (HUẤN LUYỆN / TÙY BIẾN) CLOUD AI
=============================================================================
Mục đích:
  - Cho phép người dùng hoặc kỹ sư công nghiệp "dạy" (huấn luyện tri thức) 
    cho Cloud AI (Google Gemini 3.6 Flash) theo ngữ cảnh dây chuyền, máy móc.
  - Tự động chạy với THUẦN PYTHON (sử dụng urllib chuẩn của Python, không bắt buộc
    cài đặt thêm thư viện ngoài).
  - Tự động đọc API Key từ file src/main.cpp hoặc biến môi trường GEMINI_API_KEY.
=============================================================================
"""

import os
import sys
import json
import urllib.request
import urllib.error

# Thiết lập UTF-8 cho Windows console
if sys.stdout.encoding != 'utf-8':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except Exception:
        pass

# =============================================================================
# BƯỚC 1: TỰ ĐỘNG TÌM API KEY TỪ MAIN.CPP HOẶC MÔI TRƯỜNG
# =============================================================================
def get_gemini_api_key():
    # 1. Thử đọc từ biến môi trường
    key = os.getenv("GEMINI_API_KEY", "")
    if key and not key.startswith("YOUR_"):
        return key

    # 2. Tự động đọc từ src/main.cpp của dự án
    main_path = os.path.join(os.path.dirname(__file__), "..", "src", "main.cpp")
    if os.path.exists(main_path):
        try:
            with open(main_path, "r", encoding="utf-8") as f:
                for line in f:
                    if "GEMINI_API_KEY" in line and "=" in line and ";" in line:
                        extracted = line.split("=")[1].split(";")[0].strip().strip('"').strip("'")
                        if extracted and not extracted.startswith("YOUR_") and len(extracted) > 10:
                            return extracted
        except Exception:
            pass

    return ""

# =============================================================================
# BƯỚC 2: TRI THỨC CHUYÊN SÂU NHÀ MÁY (DOMAIN KNOWLEDGE & SỔ TAY BẢO TRÌ)
# =============================================================================
PLANT_DOMAIN_KNOWLEDGE = """
[CẨM NANG VẬN HÀNH & MÃ LỖI ĐỘNG CƠ CÔNG NGHIỆP - DÂY CHUYỀN SỐ 1]
- Loại thiết bị: Động cơ 3 pha 380V - 15kW trên bo mạch ESP32-S3.
- Tiêu chuẩn rung động ISO 10816-3:
  + RMS < 2.8 mm/s: Vùng A/B (Hoạt động tốt, an toàn tuyệt đối).
  + RMS 2.8 - 4.5 mm/s: Vùng C (Cảnh báo mòn bạc đạn, cho phép chạy có giám sát).
  + RMS > 7.1 mm/s: Vùng D (Nguy hiểm cấp bách - Bắt buộc dừng máy bảo dưỡng).
- Nhiệt độ thân máy cho phép: Max 75 độ C. Nếu > 80 độ C kèm rung động cao: Nguy cơ bó kẹt bạc đạn.
- Quy trình ứng cứu khẩn cấp:
  + Nếu RMS > 7.1 mm/s hoặc Nhiệt độ > 85 độ C: Ngay lập tức phát lệnh ngắt Relay 1 [CMD:RELAY1_OFF] 
    và kích hoạt đèn LED cảnh báo [CMD:LED_BLINK] để công nhân hiện trường nhận biết.
"""

# Hướng dẫn vai trò & Thao tác phần cứng
SYSTEM_INSTRUCTION = f"""
Bạn là chuyên gia chẩn đoán sự cố công nghiệp cấp cao tích hợp trong hệ thống AIoT_LIB.
Bạn giám sát bo mạch vi điều khiển ESP32-S3 kết nối các cảm biến và cơ cấu chấp hành.

Tri thức chuyên sâu của nhà máy:
{PLANT_DOMAIN_KNOWLEDGE}

Nguyên tắc điều khiển phần cứng qua thẻ lệnh (Action Calling):
- Bật đèn LED onboard: đính kèm [CMD:LED_ON]
- Tắt đèn LED onboard: đính kèm [CMD:LED_OFF]
- Chớp nháy đèn LED khẩn cấp: đính kèm [CMD:LED_BLINK]
- Đóng Relay 1 (Cấp điện động cơ): đính kèm [CMD:RELAY1_ON]
- Ngắt Relay 1 (Cắt nguồn bảo vệ): đính kèm [CMD:RELAY1_OFF]

Hãy suy luận sắc bén theo ISO 10816-3, trả lời súc tích bằng tiếng Việt và luôn thực hiện đúng lệnh phần cứng.
"""

# =============================================================================
# BƯỚC 3: GỬI TRUY VẤN TỚI GOOGLE GEMINI (REST API CHUẨN)
# =============================================================================
def ask_gemini_rest(prompt, api_key, model="gemini-3.5-flash-lite"):
    url = f"https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent?key={api_key}"
    payload = {
        "contents": [
            {
                "parts": [{"text": prompt}]
            }
        ],
        "systemInstruction": {
            "parts": [{"text": SYSTEM_INSTRUCTION}]
        }
    }
    
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"})
    
    try:
        with urllib.request.urlopen(req, timeout=20) as response:
            res_data = json.loads(response.read().decode("utf-8"))
            candidates = res_data.get("candidates", [])
            if candidates:
                parts = candidates[0].get("content", {}).get("parts", [])
                if parts:
                    return parts[0].get("text", "").strip()
            return "Không nhận được nội dung phản hồi từ Gemini."
    except urllib.error.HTTPError as e:
        err_msg = e.read().decode("utf-8")
        return f"[HTTP Error {e.code}]: {err_msg}"
    except Exception as e:
        return f"[Lỗi kết nối]: {e}"

# =============================================================================
# BƯỚC 4: CHƯƠNG TRÌNH KIỂM THỬ KHẢ NĂNG SUY LUẬN CỦA CLOUD AI
# =============================================================================
def test_cloud_ai():
    print("=================================================================")
    print("      KHOI TAO VA KIEM THU TRI TUE NHAN TAO CLOUD AI            ")
    print("=================================================================")

    api_key = get_gemini_api_key()
    if not api_key:
        print("\n[ERROR] Chua tim thay GEMINI_API_KEY!")
        print(" - Vui long mo file src/main.cpp va dien API Key vao dong:")
        print('   const char *GEMINI_API_KEY = "AIzaSy...";')
        print("   hoac dat bien moi truong: set GEMINI_API_KEY=AIzaSy...")
        return

    print(f"[*] Su dung API Key: {api_key[:6]}...{api_key[-4:]}")
    print("[*] Dang ket noi toi mo hinh: gemini-3.5-flash-lite...")
    print("[*] Da nap thanh cong bo Tri thuc chuyen sau ISO 10816-3 & Action Protocol!\n")

    test_queries = [
        "Động cơ đang chạy với độ rung RMS = 1.6 mm/s, nhiệt độ 41 C. Đánh giá trạng thái giúp tôi.",
        "Tôi cần ánh sáng soi buồng máy, bạn bật đèn led onboard trên mạch ESP32 lên giúp tôi.",
        "Cảnh báo khẩn! Rung RMS đột biến lên 9.2 mm/s, nhiệt độ 87 C, có hiện tượng quá tải!"
    ]

    for q in test_queries:
        print(f"\n[USER_QUERY]: {q}")
        reply = ask_gemini_rest(q, api_key)
        print(f"[CLOUD_AI_RESPONSE]:\n{reply}")
        print("-" * 65)

if __name__ == "__main__":
    test_cloud_ai()
