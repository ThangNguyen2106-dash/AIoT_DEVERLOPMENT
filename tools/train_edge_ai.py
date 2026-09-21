"""
=============================================================================
           AIoT_LIB - HƯỚNG DẪN HUẤN LUYỆN (DẠY) EDGE AI (TINYML)
=============================================================================
Mục đích:
  - Cho phép người dùng hoặc nhà nghiên cứu tự dạy (train) mô hình Edge AI 
    theo dữ liệu cảm biến thực tế của riêng mình (rung động, nhiệt độ, áp suất...).
  - Huấn luyện mô hình phân loại đa lớp (Normal, Warning, Critical...).
  - TỰ ĐỘNG XUẤT RA FILE C++ HEADER (.h) chứa ma trận trọng số (Weights W, Biases b)
    để copy và chạy trực tiếp trên ESP32 / ESP32-S3 với tốc độ siêu nhanh (< 1ms).

Tương thích:
  - Chạy được ở chế độ THUẦN PYTHON (không cần cài thêm thư viện phụ).
  - Tự động nâng cấp lên scikit-learn nếu máy có sẵn (pip install scikit-learn numpy).
=============================================================================
"""

import os
import sys
import math
import random

# Thiết lập UTF-8 cho Windows console
if sys.stdout.encoding != 'utf-8':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except Exception:
        pass

try:
    import numpy as np
    from sklearn.linear_model import LogisticRegression
    from sklearn.model_selection import train_test_split
    from sklearn.metrics import classification_report, accuracy_score
    HAS_SKLEARN = True
except ImportError:
    HAS_SKLEARN = False

# =============================================================================
# BƯỚC 1: DỮ LIỆU CẢM BIẾN MẪU (DATASET)
# =============================================================================
def get_sample_data():
    """
    Tạo dữ liệu trích xuất từ cảm biến rung động động cơ:
    - Feature 0: Giá trị trung bình (Mean)
    - Feature 1: Căn quân phương (RMS)
    - Feature 2: Đỉnh - Đáy (Peak-to-Peak)
    - Feature 3: Độ lệch chuẩn (Standard Deviation)
    
    Nhãn (Labels):
    - 0: NORMAL (Hoạt động bình thường)
    - 1: WARNING (Bạc đạn mòn, rung lắc tăng)
    - 2: CRITICAL (Kẹt trục, nguy hiểm cấp bách)
    """
    random.seed(42)
    samples = []
    # 0: NORMAL (RMS ~ 1.5 - 2.5)
    for _ in range(400):
        mean_v = random.gauss(0.0, 0.2)
        rms_v = random.gauss(1.8, 0.3)
        p2p_v = random.gauss(4.5, 0.8)
        std_v = random.gauss(0.5, 0.1)
        samples.append(([mean_v, rms_v, p2p_v, std_v], 0))
    # 1: WARNING (RMS ~ 3.8 - 5.5)
    for _ in range(400):
        mean_v = random.gauss(0.5, 0.3)
        rms_v = random.gauss(4.5, 0.5)
        p2p_v = random.gauss(11.0, 1.5)
        std_v = random.gauss(1.4, 0.2)
        samples.append(([mean_v, rms_v, p2p_v, std_v], 1))
    # 2: CRITICAL (RMS ~ 8.0 - 15.0)
    for _ in range(400):
        mean_v = random.gauss(1.5, 0.6)
        rms_v = random.gauss(9.5, 1.2)
        p2p_v = random.gauss(25.0, 3.0)
        std_v = random.gauss(3.2, 0.4)
        samples.append(([mean_v, rms_v, p2p_v, std_v], 2))
        
    random.shuffle(samples)
    return samples

# =============================================================================
# BƯỚC 2: HUẤN LUYỆN (DẠY) MÔ HÌNH TINYML
# =============================================================================
def train_tinyml():
    print("=================================================================")
    print("      KHOI DONG QUA TRINH HUAN LUYEN (TRAINING) EDGE AI         ")
    print("=================================================================")
    
    samples = get_sample_data()
    num_features = 4
    num_classes = 3
    
    if HAS_SKLEARN:
        print("[+] Phát hiện thư viện scikit-learn & numpy: Dùng thuật toán L-BFGS tối ưu...")
        X = np.array([s[0] for s in samples])
        y = np.array([s[1] for s in samples])
        X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)
        
        clf = LogisticRegression(multi_class='multinomial', solver='lbfgs', max_iter=500)
        clf.fit(X_train, y_train)
        acc = accuracy_score(y_test, clf.predict(X_test))
        print(f"[*] Độ chính xác mô hình: {acc * 100:.2f}%\n")
        W = clf.coef_.tolist()
        b = clf.intercept_.tolist()
    else:
        print("[+] Chế độ Thuần Python (Pure Python): Huấn luyện Softmax Classifier qua Gradient Descent...")
        # Khởi tạo trọng số
        W = [[0.0 for _ in range(num_features)] for _ in range(num_classes)]
        b = [0.0 for _ in range(num_classes)]
        
        lr = 0.01
        epochs = 200
        
        for epoch in range(epochs):
            for x, y in samples:
                # Forward pass: logits = W * x + b
                logits = [sum(W[c][j] * x[j] for j in range(num_features)) + b[c] for c in range(num_classes)]
                max_l = max(logits)
                exp_l = [math.exp(l - max_l) for l in logits]
                sum_exp = sum(exp_l)
                probs = [e / sum_exp for e in exp_l]
                
                # Backward pass (Cross-Entropy Gradient)
                for c in range(num_classes):
                    target = 1.0 if c == y else 0.0
                    error = probs[c] - target
                    for j in range(num_features):
                        W[c][j] -= lr * error * x[j]
                    b[c] -= lr * error
                    
        # Kiểm tra độ chính xác
        correct = 0
        for x, y in samples:
            logits = [sum(W[c][j] * x[j] for j in range(num_features)) + b[c] for c in range(num_classes)]
            pred = logits.index(max(logits))
            if pred == y:
                correct += 1
        print(f"[*] Độ chính xác mô hình: {(correct / len(samples)) * 100:.2f}%\n")
        
    print(f"[*] Đã tính toán xong ma trận trọng số W [{num_classes}x{num_features}] và vector Bias b [{num_classes}]!")
    export_to_cpp_header(W, b, output_path="src/EdgeAI/Models/CustomModelWeights.h")

# =============================================================================
# BƯỚC 3: XUẤT RA FILE C++ HEADER ĐỂ NHÚNG VÀO ESP32
# =============================================================================
def export_to_cpp_header(W, b, output_path):
    num_classes = len(W)
    num_features = len(W[0])
    
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    
    header_content = f"""// ============================================================================
// File tự động sinh bởi công cụ huấn luyện: tools/train_edge_ai.py
// Ma trận trọng số TinyML cho bộ phân loại Edge AI trên ESP32 / ESP32-S3
// ============================================================================
#ifndef CUSTOM_MODEL_WEIGHTS_H
#define CUSTOM_MODEL_WEIGHTS_H

#include <stddef.h>

namespace EdgeModels
{{
    namespace CustomModel
    {{
        constexpr size_t NUM_FEATURES = {num_features};
        constexpr size_t NUM_CLASSES = {num_classes};

        // Ma trận trọng số W [{num_classes} x {num_features}]
        static const float W[{num_classes}][{num_features}] = {{
"""
    for i in range(num_classes):
        row_str = ", ".join([f"{val:.6f}f" for val in W[i]])
        header_content += f"            {{{row_str}}}"
        if i < num_classes - 1:
            header_content += ",\n"
        else:
            header_content += "\n"
            
    header_content += f"""        }};

        // Vector độ lệch Bias b [{num_classes}]
        static const float b[{num_classes}] = {{
            """
    bias_str = ", ".join([f"{val:.6f}f" for val in b])
    header_content += bias_str + """
        };

        // Tên các nhãn phân loại
        static const char *CLASS_NAMES[NUM_CLASSES] = {
            "NORMAL",
            "WARNING",
            "CRITICAL"
        };
    }
}

#endif /* CUSTOM_MODEL_WEIGHTS_H */
"""
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(header_content)
        
    print(f"[SUCCESS] Exported C++ Header successfully:")
    print(f" - Path: {os.path.abspath(output_path)}")
    print(" - Model ready for on-chip ESP32 TinyML neural inference (< 1ms).")

if __name__ == "__main__":
    train_tinyml()
