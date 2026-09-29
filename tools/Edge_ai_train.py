import sys
import numpy as np
from sklearn.linear_model import LogisticRegression
from sklearn.preprocessing import MinMaxScaler, StandardScaler

# Đảm bảo in tiếng Việt có dấu chuẩn UTF-8 trên Windows PowerShell / CMD
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8')

# ==============================================================================
# 1. ĐỌC VÀ CHUẨN BỊ TẬP DỮ LIỆU
# ==============================================================================
# Mỗi mẫu gồm: 12 đặc trưng (4 đặc trưng x 3 kênh cảm biến) + 1 nhãn (Label Y)
# BẠN HÃY NẠP TẬP DỮ LIỆU CỦA BẠN VÀO ĐÂY HOẶC ĐỌC TỪ FILE CSV:
# Ví dụ đọc file CSV:
# raw_data = np.genfromtxt('dataset.csv', delimiter=',')
raw_data = np.array([
    # --- Nhãn 0: Trạng thái bình thường (NORMAL) ---
    [25.91, 25.92, 2.56, 0.73, 59.25, 59.28, 5.72, 1.65, 850.89, 851.28, 70.88, 26.95, 0],
    [25.94, 25.95, 2.56, 0.71, 59.35, 59.37, 5.81, 1.61, 851.57, 851.94, 70.88, 25.91, 0],
    [25.97, 25.98, 2.56, 0.69, 59.44, 59.46, 5.82, 1.58, 847.51, 847.97, 91.15, 29.17, 0],
    
    # --- Nhãn 1: Cảnh báo quá nhiệt / quá tải (WARNING) ---
    [38.50, 38.60, 4.20, 1.10, 68.20, 68.50, 7.80, 2.30, 920.10, 921.00, 85.00, 31.20, 1],
    [39.10, 39.20, 4.35, 1.15, 69.10, 69.30, 8.10, 2.45, 925.40, 926.10, 86.50, 32.10, 1],
    [39.80, 39.90, 4.50, 1.20, 70.00, 70.20, 8.40, 2.60, 930.00, 931.20, 88.00, 33.00, 1],
    
    # --- Nhãn 2: Nguy cấp / Sự cố (CRITICAL) ---
    [48.00, 48.20, 8.90, 3.50, 82.00, 82.50, 15.2, 5.80, 980.00, 982.50, 120.0, 45.00, 2],
    [49.50, 49.80, 9.20, 3.80, 84.10, 84.80, 16.0, 6.10, 985.20, 988.00, 125.5, 47.20, 2],
    [51.00, 51.30, 9.60, 4.10, 86.00, 86.70, 16.8, 6.40, 990.10, 993.40, 130.0, 49.00, 2],
])

# Tách đặc trưng X (12 cột đầu) và nhãn Y (cột cuối)
X_raw = raw_data[:, :12]
y = raw_data[:, 12].astype(int)

# Kiểm tra điều kiện tiên quyết: Mô hình phân loại cần ít nhất 2 nhãn
unique_labels = np.unique(y)
if len(unique_labels) < 2:
    print(f"\n[LỖI]: Bạn chỉ có {len(unique_labels)} nhãn (Label: {unique_labels}) trong tập dữ liệu!")
    print("Mô hình phân loại cần ít nhất 2 nhãn khác nhau (Ví dụ: 0 là BÌNH THƯỜNG, 1 là QUÁ TẢI).")
    print("Hãy bổ sung thêm các mẫu của nhãn khác vào mảng raw_data để tiếp tục huấn luyện.\n")
    sys.exit(1)

num_classes = len(unique_labels)
input_dim = X_raw.shape[1]

def train_and_extract_weights(X_scaled, y_train):
    """Huấn luyện hồi quy Logistic và chuẩn hóa ma trận trọng số cho ESP32"""
    clf = LogisticRegression(solver='lbfgs', max_iter=1000)
    clf.fit(X_scaled, y_train)
    W = clf.coef_
    b = clf.intercept_
    # Xử lý trường hợp nhị phân 2 lớp (scikit-learn chỉ trả về 1 hàng thay vì 2)
    if num_classes == 2 and W.shape[0] == 1:
        W = np.vstack([-W, W])
        b = np.array([-b[0], b[0]])
    return W.flatten(), b

# ==============================================================================
# PHƯƠNG ÁN 1: CHUẨN HÓA Z-SCORE (NORM_Z_SCORE)
# Khuyên dùng: Chống nhiễu ngoại lai, cực kỳ tốt cho cảm biến rung động, dòng, âm thanh
# ==============================================================================
scaler_z = StandardScaler()
X_zscore = scaler_z.fit_transform(X_raw)
mean_vals = scaler_z.mean_
std_vals = np.where(scaler_z.scale_ < 1e-6, 1.0, scaler_z.scale_)
W_zscore, b_zscore = train_and_extract_weights(X_zscore, y)

# ==============================================================================
# PHƯƠNG ÁN 2: CHUẨN HÓA MIN-MAX (NORM_MIN_MAX)
# Phù hợp: Dữ liệu có biên độ vật lý xác định rõ như dải đo nhiệt độ, độ ẩm (0..100)
# ==============================================================================
scaler_mm = MinMaxScaler(feature_range=(0, 1))
X_minmax = scaler_mm.fit_transform(X_raw)
min_vals = scaler_mm.data_min_
max_vals = scaler_mm.data_max_
W_minmax, b_minmax = train_and_extract_weights(X_minmax, y)

# ==============================================================================
# XUẤT MÃ NGUỒN C++ ĐỂ BẠN TÙY Ý LỰA CHỌN NẠP VÀO ESP32
# ==============================================================================
print(f"// ==============================================================================")
print(f"// THÔNG SỐ CỐ ĐỊNH CHUNG")
print(f"// ==============================================================================")
print(f"constexpr size_t INPUT_DIM = {input_dim};")
print(f"constexpr size_t NUM_LABELS = {num_classes};\n")

print(f"// ==============================================================================")
print(f"// [LỰA CHỌN 1]: CHUẨN HÓA Z-SCORE (meanVals, stdDevVals)")
print(f"// Ưu điểm: Tối ưu cho tín hiệu rung chấn, dòng điện, âm thanh chống nhiễu")
print(f"// ==============================================================================")
print(f"float meanVals[{input_dim}] = {{ {', '.join([f'{m:.4f}f' for m in mean_vals])} }};")
print(f"float stdDevVals[{input_dim}] = {{ {', '.join([f'{s:.4f}f' for s in std_vals])} }};")
print(f"float W_zscore[] = {{ " + ", ".join([f"{w:.6f}f" for w in W_zscore]) + " };")
print(f"float b_zscore[] = {{ " + ", ".join([f"{bias:.6f}f" for bias in b_zscore]) + " };\n")
print(f"// Cách nạp vào ESP32 trong hàm setup():")
print(f"//   AIoT.edgeAI.setModel(W_zscore, b_zscore, INPUT_DIM, NUM_LABELS, 0);")
print(f"//   AIoT.edgeAI.setZScore(meanVals, stdDevVals);\n")

print(f"// ==============================================================================")
print(f"// [LỰA CHỌN 2]: CHUẨN HÓA MIN-MAX (minValues, maxValues)")
print(f"// Ưu điểm: Phù hợp dữ liệu nhiệt độ, độ ẩm có dải biên vật lý rõ ràng")
print(f"// ==============================================================================")
print(f"float minValues[{input_dim}] = {{ {', '.join([f'{m:.4f}f' for m in min_vals])} }};")
print(f"float maxValues[{input_dim}] = {{ {', '.join([f'{m:.4f}f' for m in max_vals])} }};")
print(f"float W_minmax[] = {{ " + ", ".join([f"{w:.6f}f" for w in W_minmax]) + " };")
print(f"float b_minmax[] = {{ " + ", ".join([f"{bias:.6f}f" for bias in b_minmax]) + " };\n")
print(f"// Cách nạp vào ESP32 trong hàm setup():")
print(f"//   AIoT.edgeAI.setModel(W_minmax, b_minmax, INPUT_DIM, NUM_LABELS, 0);")
print(f"//   AIoT.edgeAI.setNormalization(minValues, maxValues);")