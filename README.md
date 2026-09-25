# Thư Viện AIoT (AIoT_LIB)

<p align="center">
  <b>Framework AIoT Mã Nguồn Mở Hiệu Năng Cao Cho Các Dòng Vi Điều Khiển ESP32 & Đa Nền Tảng Cloud / AI</b>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-ESP32%20%7C%20ESP32--S2%20%7C%20ESP32--S3%20%7C%20ESP32--C3%20%7C%20ESP32--C6-blue?style=for-the-badge&logo=espressif" alt="ESP32 Chips" />
  <img src="https://img.shields.io/badge/Framework-Arduino%20%7C%20PlatformIO-orange?style=for-the-badge&logo=platformio" alt="Framework" />
  <img src="https://img.shields.io/badge/Security-TLS%2FSSL%20Port%208883-green?style=for-the-badge&logo=letsencrypt" alt="TLS Security" />
  <img src="https://img.shields.io/badge/Edge%20AI-Signal%20Preprocessing%20%26%20Feature%20Vector-red?style=for-the-badge" alt="Edge AI" />
  <img src="https://img.shields.io/badge/Cloud%20AI-Google%20Gemini%20Flash-purple?style=for-the-badge&logo=google" alt="AI Ready" />
  <img src="https://img.shields.io/badge/License-GPLv3-blue?style=for-the-badge" alt="License" />
</p>

---

## Mục Lục

1. [Giới Thiệu & Triết Lý Thiết Kế (Introduction & Philosophy)](#1-giới-thiệu--triết-lý-thiết-kế-introduction--philosophy)
2. [Các Ứng Dụng Thực Tế Tiêu Biểu (Real-World Applications)](#2-các-ứng-dụng-thực-tế-tiêu-biểu-real-world-applications)
3. [Kiến Trúc 4 Tầng & Nguyên Lý Vận Hành (Architecture)](#3-kiến-trúc-4-tầng--nguyên-lý-vận-hành-architecture)
   - [Tầng 1: IoT Core & Trừu Tượng Hóa Thiết Bị Ngoại Vi (HAL)](#tầng-1-iot-core--trừu-tượng-hóa-thiết-bị-ngoại-vi-hal)
   - [Tầng 2: Tiền Xử Lý & Trích Xuất Vector Đặc Trưng Tại Biên (Edge AI)](#tầng-2-tiền-xử-lý--trích-xuất-vector-đặc-trưng-tại-biên-edge-ai)
   - [Tầng 3: Trí Tuệ Nhân Tạo Đám Mây (Cloud AI - Google Gemini)](#tầng-3-trí-tuệ-nhân-tạo-đám-mây-cloud-ai---google-gemini)
   - [Tầng 4: Hệ Thống Điện Toán Lai (Hybrid AI Orchestration)](#tầng-4-hệ-thống-điện-toán-lai-hybrid-ai-orchestration)
4. [Hướng Dẫn Bắt Đầu Nhanh (Quickstart)](#4-hướng-dẫn-bắt-đầu-nhanh-quickstart)
5. [Bảng Tra Cứu API Tóm Tắt (API Reference)](#5-bảng-tra-cứu-api-tóm-tắt-api-reference)
6. [Tác Quyền & Giấy Phép (License)](#6-tác-quyền--giấy-phép-license)

---

## 1. Giới Thiệu & Triết Lý Thiết Kế (Introduction & Philosophy)

Trong phát triển hệ thống nhúng hiện đại, hai mô hình truyền thống bộc lộ những hạn chế rõ rệt:
- **Hệ thống IoT thuần túy**: Thiết bị chỉ đơn thuần đọc cảm biến và gửi lên máy chủ (Sensor Node), không có năng lực tự xử lý hay phản ứng khi mất mạng.
- **Hệ thống Cloud AI thuần túy**: Phụ thuộc 100% vào kết nối Internet. Khi mạng chập chờn hoặc độ trễ phản hồi từ Cloud lên đến 2-3 giây, các sự cố chập tải hay kẹt động cơ đã có thể gây hư hỏng trước khi nhận được phản hồi.

**AIoT_LIB** kết hợp sức mạnh của cả hai nền tảng theo mô hình **Điện toán lai (Hybrid AI Architecture)**:
> *"Tiền xử lý & trích xuất đặc trưng tại biên siêu tốc ($< 1\text{ms}$) — Tư duy phân tích sâu trên đám mây như bộ não trung tâm."*

Thư viện được thiết kế tối ưu cho các dòng chip **ESP32 (ESP32-S3, ESP32-C3, ESP32 standard)**, độc lập phần cứng và không phụ thuộc vào bất kỳ thư viện bên ngoài nào cho phần tính toán toán học (Zero-Dependency AI Math).

---

## 2. Các Ứng Dụng Thực Tế Tiêu Biểu (Real-World Applications)

### 2.1. Công Nghiệp 4.0 & Bảo Trì Dự Đoán (Predictive Maintenance)
* **Tầng biên (Edge AI)**: Thu thập chuỗi thời gian qua Cửa sổ trượt `SlidingWindow`, lọc nhiễu qua bộ lọc Kalman 1D, trích xuất vector đặc trưng 4 chiều (Mean, RMS, Peak-to-Peak, StdDev) đã chuẩn hóa.
* **Tầng đám mây (Cloud AI)**: Gửi thông số vận hành lên Google Gemini để chẩn đoán nguyên nhân gốc rễ và đưa ra khuyến nghị bảo trì cho kỹ sư vận hành.

### 2.2. Nhà Thông Minh Trò Chuyện & Điều Khiển Tự Nhiên (Smart Home & Assistant)
* Người dùng nói: *"Trời oi bức quá, bật quạt lên giúp tôi nhé"*.
* Gemini hiểu ngữ cảnh và chèn tag lệnh an toàn `[CMD:RELAY1_ON]`.
* ESP32-S3 kiểm tra Whitelist, kích hoạt chân GPIO và phản hồi thân thiện.

---

## 3. Kiến Trúc 4 Tầng & Nguyên Lý Vận Hành (Architecture)

```mermaid
flowchart TD
    subgraph Layer4["TẦNG 4: HYBRID AI ORCHESTRATOR"]
        Hybrid["HybridAIEngine<br/>(Điều phối thông minh: Edge-First, Cloud-Assist)"]
    end

    subgraph Layer3["TẦNG 3: CLOUD AI (GOOGLE GEMINI)"]
        Gemini["GeminiClient (HTTPS REST)<br/>Hiểu ngôn ngữ tự nhiên | Function Calling [CMD:...]"]
    end

    subgraph Layer2["TẦNG 2: EDGE AI & AI_MATH"]
        Edge["Signal Preprocessing & Feature Extraction<br/>Kalman, SlidingWindow, FFT | Trích xuất & Chuẩn hóa Vector"]
    end

    subgraph Layer1["TẦNG 1: IOT CORE & HAL PHẦN CỨNG"]
        Device["AIoT_Device (Quản lý chân động: Relay, LED, Buzzer, RGB Pin 48)"]
        IoTNet["AIoT Core (WiFi Smart Captive Portal + MQTT TLS 8883)"]
    end

    Layer1 --> Layer2
    Layer2 --> Layer4
    Layer3 --> Layer4
    Layer4 --> Device
```

### Tầng 1: IoT Core & Trừu Tượng Hóa Thiết Bị Ngoại Vi (HAL)
- **Độc lập phần cứng (Dynamic Pin Mapping)**: Không gán cố định chân GPIO trong thư viện. Bạn đăng ký chân tự do lúc runtime bằng `AIoT.device.Relay(pin, "Tên")`.
- **Smart Captive Portal (Web PnP)**: Nếu không có cấu hình WiFi, bo mạch tự phát Access Point `AIoT_WiFi` (IP: `192.168.21.6`) để người dùng quét và cài đặt WiFi ngay trên trình duyệt điện thoại mà không cần nạp lại code.
- **MQTT TLS 8883**: Kết nối bảo mật HiveMQ Cloud Broker và giao tiếp hai chiều qua macro `Virtual_WRITE()`.

### Tầng 2: Tiền Xử Lý & Trích Xuất Vector Đặc Trưng Tại Biên (Edge AI)
- **Signal Preprocessing**:
  - `SlidingWindow<N>`: Cửa sổ trượt FIFO lưu $N$ mẫu tín hiệu thời gian thực trong RAM tĩnh.
  - `KalmanFilter1D`: Bộ lọc Kalman 1 chiều khử nhiễu đo tối ưu.
  - `MovingAverageFilter` & `LowPassFilter`: Bộ lọc trung bình trượt và lọc thông thấp IIR.
  - `FastFourierTransform`: Biến đổi Fourier Cooley-Tukey Radix-2 phân tích phổ tần số.
- **Feature Extraction & Normalization**:
  - Trích xuất 4 chỉ số thống kê cốt lõi: Mean, RMS, Peak-to-Peak, StdDev.
  - Chuẩn hóa Min-Max scaling về dải $[0.0, 1.0]$ hoặc chuẩn hóa Z-Score.
  - Đóng gói dữ liệu ra vector chuẩn hóa `FeatureVector` sẵn sàng cho các mô hình suy luận người dùng tự phát triển.

### Tầng 3: Trí Tuệ Nhân Tạo Đám Mây (Cloud AI - Google Gemini)
- **Function Calling qua Tag Injection**: AI không bao giờ điều khiển trực tiếp phần cứng. Gemini phản hồi văn bản kèm các thẻ lệnh như `[CMD:RELAY1_ON]`.
- **Bộ phân giải lệnh an toàn (Whitelist Command Parser)**: ESP32-S3 nhận chuỗi phản hồi, quét các tag `[CMD:...]`, kiểm tra trong danh mục cho phép rồi mới kích hoạt điện áp GPIO.
- **Chống ảo giác cảm biến (Anti-Hallucination)**: System Prompt quy định nghiêm ngặt cấm AI tự ý bịa ra số liệu nhiệt độ, độ ẩm khi hệ thống chưa kết nối cảm biến.

### Tầng 4: Hệ Thống Điện Toán Lai (Hybrid AI Orchestration)
- Tích hợp cả Edge AI và Cloud AI dưới sự điều phối của `PolicyEngine` và `AIOrchestrator`.
- Định tuyến công việc thông minh: Edge-First cho phản ứng tức thì, Cloud-Assist cho phân tích chuyên sâu.

---

## 4. Hướng Dẫn Bắt Đầu Nhanh (Quickstart)

### Bước 1: Chuẩn bị môi trường
1. Cài đặt **VSCode** và tiện ích mở rộng **PlatformIO IDE**.
2. Mở thư mục dự án `AIoT_LIB`.

### Bước 2: Cấu hình API Key bảo mật
Tạo file `src/secrets.h` (file này đã có trong `.gitignore` để bảo vệ key không bị lộ lên Git):
```cpp
#pragma once
const char *GEMINI_API_KEY = "AIzaSy_YOUR_GOOGLE_GEMINI_API_KEY";
```

### Bước 3: Biên dịch & Chạy mã nguồn
1. Bấm nút **Build** (hoặc gõ lệnh `pio run`).
2. Kết nối bo mạch ESP32-S3 qua cáp USB và bấm **Upload and Monitor** (hoặc `pio run -t upload -t monitor`).
3. Bo mạch sẽ tự động tiền xử lý tín hiệu, trích xuất và hiển thị vector đặc trưng đã chuẩn hóa ra Serial Monitor.

---

## 5. Bảng Tra Cứu API Tóm Tắt (API Reference)

Toàn bộ hệ sinh thái được gom gọn và truy cập thông qua đối tượng toàn cục duy nhất **`AIoT`**:

### 1. Phân Hệ IoT Core (`AIoT.*`)
| Cú pháp | Kiểu trả về | Chức năng |
| :--- | :--- | :--- |
| `AIoT.begin(ssid, pass, mqtt_user, mqtt_pass)` | `void` | Khởi động WiFi và kết nối MQTT TLS 8883. Nếu `ssid` rỗng, phát Web AP `192.168.21.6`. |
| `AIoT.run()` | `void` | Duy trì kết nối mạng, xử lý Web Portal, lắng nghe gói tin (bắt buộc gọi trong `loop()`). |
| `AIoT.updateTelemetry(key, val)` | `void` | Nạp một thông số vào bộ đệm Telemetry JSON. |
| `AIoT.sendTelemetry()` | `bool` | Đóng gói toàn bộ thông số trong bộ đệm và gửi lên Cloud trong 1 bản tin duy nhất. |
| `Virtual_WRITE(vPin)` | Macro | Khai báo hàm callback nhận lệnh điều khiển từ xa từ App/Dashboard. |

### 2. Tiền Xử Lý & Trích Xuất Vector Tại Biên (`AIoT.edgeAI.*` & `AI_Math`)
| Cú pháp | Kiểu trả về | Chức năng |
| :--- | :--- | :--- |
| `AIoT.edgeAI.push(sample)` | `void` | Đẩy một điểm dữ liệu cảm biến mới vào Cửa sổ trượt. |
| `AIoT.edgeAI.process(sample)` | `EdgeAI::FeatureVector` | Đẩy mẫu và trích xuất ngay vector đặc trưng 4 chiều đã chuẩn hóa. |
| `AIoT.edgeAI.extractNormalizedVector()` | `EdgeAI::FeatureVector` | Lấy vector đặc trưng đã chuẩn hóa từ cửa sổ trượt hiện tại. |
| `AIoT.edgeAI.setMinMaxRanges(minVals, maxVals)` | `void` | Cấu hình dải giá trị Min/Max cho từng đặc trưng phục vụ chuẩn hóa. |
| `AIoT.edgeAI.isReady()` | `bool` | Kiểm tra cửa sổ trượt đã thu thập đủ mẫu tối thiểu hay chưa. |
| `AI_Math::FeatureExtraction::extractVector(...)` | `void` | Trích xuất 4 đặc trưng thô `[mean, rms, p2p, stdDev]` từ mảng mẫu. |
| `AI_Math::FeatureExtraction::normalizeMinMax(...)` | `void` | Chuẩn hóa mảng đặc trưng về dải $[0.0, 1.0]$. |
| `AI_Math::FeatureExtraction::normalizeZScore(...)` | `void` | Chuẩn hóa mảng đặc trưng theo Z-Score $\frac{x - \mu}{\sigma}$. |

### 3. Trí Tuệ Đám Mây (`AIoT.cloudAI.*`)
| Cú pháp | Kiểu trả về | Chức năng |
| :--- | :--- | :--- |
| `AIoT.cloudAI.setApiKey(key)` | `void` | Cấu hình khóa API Google Gemini (AI Studio). |
| `AIoT.cloudAI.setModel(modelName)` | `void` | Cấu hình model Gemini (mặc định: `gemini-2.5-flash`). |
| `AIoT.cloudAI.ask(prompt, systemPrompt)` | `String` | Gửi câu hỏi kèm System Prompt lên Google Gemini qua HTTPS và nhận phản hồi văn bản. |
| `AIoT.cloudAI.hasApiKey()` | `bool` | Kiểm tra xem khóa API đã được cấu hình hay chưa. |

### 4. Hệ Thống Điện Toán Lai (`AIoT.hybridAI.*`)
| Cú pháp | Kiểu trả về | Chức năng |
| :--- | :--- | :--- |
| `AIoT.hybridAI.process(sample)` | `EdgeAI::FeatureVector` | Chạy xử lý mẫu cảm biến tại Edge AI và trả về vector chuẩn hóa. |
| `AIoT.hybridAI.onFeatures(callback)` | `void` | Đăng ký callback khi có vector đặc trưng mới. |
| `AIoT.hybridAI.orchestrator.process(input)` | `AIResult` | Điều phối luồng xử lý thông minh theo chính sách `PolicyEngine`. |

### 5. Quản Lý Phần Cứng HAL (`AIoT.device.*`)
| Cú pháp | Ví dụ | Chức năng |
| :--- | :--- | :--- |
| `AIoT.device.Relay(pin, label)` | `AIoT.device.Relay(14, "Quạt")` | Đăng ký rơ-le tại chân GPIO chỉ định. |
| `.on()`, `.off()`, `.toggle()` | `AIoT.device.Relay(14).on();` | Bật, tắt, hoặc đảo trạng thái rơ-le. |
| `.rgb(r, g, b)` | `AIoT.device.rgb(0, 255, 0);` | Đổi màu LED RGB (GPIO 48). |
| `.beep(ms)` | `AIoT.device.beep(100);` | Kêu bíp còi Buzzer theo thời gian mili-giây. |
| `executeCommand(str)` | `AIoT.device.executeCommand(reply);` | Tự động quét và thực thi các tag lệnh `[CMD:RELAY:...]`, `[CMD:BEEP]`. |

---

## 6. Tác Quyền & Giấy Phép (License)

Dự án được phát triển bởi **Thắng Nguyễn** và phát hành dưới giấy phép **GNU General Public License v3.0 (GPLv3)**.

Theo các điều khoản của giấy phép GPLv3:
- Mọi người có quyền tự do sử dụng, nghiên cứu, sửa đổi và chia sẻ mã nguồn phần mềm này.
- Khi phân phối hoặc phát hành các sản phẩm phái sinh từ AIoT_LIB, mã nguồn của các sửa đổi/sản phẩm đó bắt buộc phải được công khai dưới cùng giấy phép GPLv3.
- Chi tiết toàn văn giấy phép xem tại file [LICENSE](LICENSE).