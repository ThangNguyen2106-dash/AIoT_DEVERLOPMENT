#include <Arduino.h>
#include <AIoT.h>
#include <LittleFS.h>

// Định nghĩa cấu trúc Vector đầu vào cho phòng học theo kiến trúc nối đuôi (Concatenation)
#define NUM_SENSORS 4                                    // 4 cảm biến: Nhiệt độ, Độ ẩm, Ánh sáng, Áp suất
#define FEATURES_PER_SENSOR 4                            // 4 chỉ số thống kê: Mean, RMS, P2P, StdDev
#define TOTAL_INPUTS (NUM_SENSORS * FEATURES_PER_SENSOR) // 16 đặc trưng đầu vào
#define NUM_OUTPUTS 3                                    // 3 trạng thái đầu ra (Ví dụ: Bình thường, Quá đông, Sự cố)

constexpr uint32_t N_INITIAL_SAMPLES = 1000; // Số mẫu lấy ban đầu nếu Flash trống
uint32_t sampleCounter = 0;
bool isSystemTrained = false;

// Đóng gói toàn bộ Não bộ vào một Struct phẳng để ghi Flash siêu tốc
struct AI_Model_Storage
{
    uint32_t magicNumber; // Mã nhận diện file hợp lệ (0x41496F54 - "AIoT")
    uint32_t version;     // Phiên bản bộ trọng số

    float weights[NUM_OUTPUTS * TOTAL_INPUTS]; // 3 * 16 = 48 trọng số (Ma trận W)
    float bias[NUM_OUTPUTS];                   // 3 số định thiên (Mảng b)

    float normalCentroids[TOTAL_INPUTS]; // 16 mốc tham chiếu trạng thái bình thường
    float minRanges[TOTAL_INPUTS];       // 16 giá trị Min để chuẩn hóa
    float maxRanges[TOTAL_INPUTS];       // 16 giá trị Max để chuẩn hóa
};

AI_Model_Storage currentModel; // Biến toàn cục chứa mô hình đang chạy trên RAM

// ================= GIAI ĐOẠN 1: QUẢN LÝ BỘ NHỚ FLASH (LittleFS) =================

void saveModelToFlash()
{
    File file = LittleFS.open("/model.bin", "w");
    if (!file)
    {
        Serial.println(F("[ESP32-S3] LỖI: Không thể mở Flash để ghi!"));
        return;
    }
    // Ghi nguyên khối nhị phân 112 bytes từ RAM xuống Flash chỉ trong 1 lệnh
    file.write((uint8_t *)&currentModel, sizeof(AI_Model_Storage));
    file.close();
    Serial.println(F("[ESP32-S3] Đã lưu bộ trọng số mới vào Flash thành công!"));
}

bool loadModelFromFlash()
{
    if (!LittleFS.exists("/model.bin"))
    {
        return false; // File chưa tồn tại (Hệ thống mới tinh)
    }

    File file = LittleFS.open("/model.bin", "r");
    if (!file)
        return false;

    file.read((uint8_t *)&currentModel, sizeof(AI_Model_Storage));
    file.close();

    // Kiểm tra magic number xem file có hợp lệ không
    if (currentModel.magicNumber == 0x41496F54)
    {
        return true;
    }
    return false;
}

// ================= GIAI ĐOẠN 2: LẮNG NGHE CẬP NHẬT TỪ CỬA SỔ PYTHON =================

void checkSerialForNewWeights()
{
    if (Serial.available() > 0)
    {
        String msg = Serial.readStringUntil('\n');
        msg.trim();

        // Nhận lệnh cập nhật từng ô trọng số từ GUI Python: UPDATE_W,vị_trí,giá_trị
        if (msg.startsWith("UPDATE_W,"))
        {
            int firstComma = msg.indexOf(',');
            int secondComma = msg.indexOf(',', firstComma + 1);

            int index = msg.substring(firstComma + 1, secondComma).toInt();
            float newWeight = msg.substring(secondComma + 1).toFloat();

            if (index >= 0 && index < (NUM_OUTPUTS * TOTAL_INPUTS))
            {
                currentModel.weights[index] = newWeight; // Đè nóng vào RAM
            }
        }
        // Nhận lệnh chốt hạ từ GUI Python: SAVE_MODEL
        else if (msg.equals("SAVE_MODEL"))
        {
            currentModel.version++; // Tăng phiên bản não bộ
            saveModelToFlash();     // Khóa cứng vào Flash vĩnh viễn

            // Phản hồi ngược lại để hiển thị trực tiếp lên màn hình đen của GUI Python
            Serial.println("THÔNG BÁO: ĐÃ LƯU CỨNG NÃO BỘ VÀO FLASH THÀNH CÔNG!");
        }
    }
}

// ================= GIAI ĐOẠN 3: LUỒNG CHẠY CHÍNH (MAIN PIPELINE) =================

void setup()
{
    Serial.begin(115200);

    if (!LittleFS.begin(true))
    { // Tự động format Flash nếu LittleFS lỗi
        Serial.println(F("[ESP32-S3] Lỗi khởi động hệ thống tệp!"));
        return;
    }

    // Kiểm tra "Não bộ" trong Flash
    if (loadModelFromFlash())
    {
        Serial.println(F("[ESP32-S3] -> Đã tìm thấy bộ não cũ trong Flash. Hệ thống sẵn sàng hoạt động!"));
        isSystemTrained = true;
    }
    else
    {
        Serial.println(F("[ESP32-S3] WARNING: CHƯA CÓ TRỌNG SỐ BAN ĐẦU!"));
        Serial.println(F("[ESP32-S3] Hệ thống tự động chuyển sang chế độ lấy mẫu N lần..."));
        isSystemTrained = false;
        sampleCounter = 0;

        // Khởi tạo dải Min-Max biên độ mặc định để chuẩn bị lưu vết
        for (int i = 0; i < TOTAL_INPUTS; i++)
        {
            currentModel.minRanges[i] = 9999.0f;
            currentModel.maxRanges[i] = -9999.0f;
        }
    }
}

void loop()
{
    // TRƯỜNG HỢP A: HỆ THỐNG MỚI TINH - ĐANG TỰ ĐỘNG LẤY MẪU KHỞI TẠO N LẦN
    if (!isSystemTrained)
    {
        if (sampleCounter < N_INITIAL_SAMPLES)
        {

            // Giả lập đọc dữ liệu thô từ các cảm biến phòng học của bạn
            float raw_temp = random(2000, 3500) / 100.0f; // 20.00°C -> 35.00°C

            // Bạn có thể nhúng thư viện `FeatureExtraction.hpp` vào đây để tính toán
            // Giả lập cập nhật dải Min-Max cho Đặc trưng ô số 0 (Mean của Nhiệt độ)
            if (raw_temp < currentModel.minRanges[0])
                currentModel.minRanges[0] = raw_temp;
            if (raw_temp > currentModel.maxRanges[0])
                currentModel.maxRanges[0] = raw_temp;

            // In tiến độ ra Serial, cửa sổ GUI Python sẽ bắt dòng này và hiển thị cho người dùng xem
            if (sampleCounter % 100 == 0)
            {
                Serial.printf("TIẾN ĐỘ LẤY MẪU KHỞI TẠO: %d / %d\n", sampleCounter, N_INITIAL_SAMPLES);
            }

            sampleCounter++;
            delay(10); // Giãn cách lấy mẫu nền
        }
        else
        {
            // Sau khi đã gom đủ N mẫu nền, tự động cấu hình bộ khung sơ cấp
            currentModel.magicNumber = 0x41496F54; // Gán mã hợp lệ vĩnh viễn
            currentModel.version = 1;

            // Gán ma trận trọng số W và bias b mặc định ban đầu (Sẽ được Python đè lại sau)
            for (int i = 0; i < (NUM_OUTPUTS * TOTAL_INPUTS); i++)
                currentModel.weights[i] = 0.01f;
            for (int i = 0; i < NUM_OUTPUTS; i++)
                currentModel.bias[i] = 0.0f;
            for (int i = 0; i < TOTAL_INPUTS; i++)
                currentModel.normalCentroids[i] = 0.5f;

            saveModelToFlash();     // Lưu cứng bộ khung sơ cấp này xuống file /model.bin
            isSystemTrained = true; // Kích hoạt đèn xanh cho hệ thống chuyển sang Edge AI thực tế
            Serial.println(F("[ESP32-S3] Đã thiết lập xong dải chuẩn hóa nền. Chuyển sang chế độ chạy Edge AI!"));
        }
        return; // Ngắt vòng lặp loop, không cho chạy suy luận AI khi đang khởi tạo mẫu
    }

    // TRƯỜNG HỢP B: HỆ THỐNG ĐÃ SẴN SÀNG VẬN HÀNH PIPELINE EDGE AI
    // -------------------------------------------------------------------------
    // 1. Đọc dữ liệu phòng học thực tế -> Tính toán 16 đặc trưng thống kê nối đuôi
    float X_raw[TOTAL_INPUTS];
    // (Giả sử bạn đã gọi FeatureExtraction để đổ đầy 16 số vào mảng X_raw này...)

    // 2. Chạy vòng lặp chuẩn hóa Min-Max cho vector 16 phần tử đứng nối đuôi nhau
    float X_normalized[TOTAL_INPUTS];
    for (int i = 0; i < TOTAL_INPUTS; i++)
    {
        X_normalized[i] = (X_raw[i] - currentModel.minRanges[i]) / (currentModel.maxRanges[i] - currentModel.minRanges[i]);
    }

    // 3. Nhân ma trận Trọng số (W) và cộng Bias (b) từ bộ nhớ Flash đã nạp lên RAM
    float output[NUM_OUTPUTS] = {0.0f, 0.0f, 0.0f};
    for (int out = 0; out < NUM_OUTPUTS; out++)
    {
        for (int in = 0; in < TOTAL_INPUTS; in++)
        {
            // Phép tính mạng thần kinh lõi: Kết quả = Tổng(X_chuẩn_hóa * W) + b
            output[out] += X_normalized[in] * currentModel.weights[out * TOTAL_INPUTS + in];
        }
        output[out] += currentModel.bias[out];
    }

    // Giả lập in kết quả trạng thái phòng học sau khi tính toán ra Serial Monitor của GUI Python
    Serial.printf("LOG_PHÒNG: Trạng thái 0 (Bình thường): %.2f | Trạng thái 1 (Quá đông): %.2f\n", output[0], output[1]);

    // -------------------------------------------------------------------------
    // LUÔN LUÔN LẮNG NGHE ĐƯỜNG SERIAL ĐỂ NHẬN BẢN CẬP NHẬT TRỌNG SỐ TỪ PYTHON GUI
    checkSerialForNewWeights();

    delay(1000); // Tốc độ quét suy luận hệ thống 1 giây / lần
}