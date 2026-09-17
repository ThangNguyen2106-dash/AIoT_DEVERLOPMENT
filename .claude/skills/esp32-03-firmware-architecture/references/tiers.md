# Phân mức firmware

Chốt mức **trước** khi thiết kế. Sai mức là sai gốc: thừa thì dự án nhỏ chết vì rườm rà,
thiếu thì sản phẩm ra thực địa không sửa được.

## Cách phân mức

Trả lời 6 câu. Không tự trả lời thay user nếu không có dữ kiện.

| Câu hỏi | Simple | Medium | Production |
|---|---|---|---|
| Bao nhiêu thiết bị chạy? | 1–vài | chục | trăm/nghìn+ |
| Có OTA không? | không | có thể | bắt buộc |
| Sửa lỗi tại chỗ được không? | cắm cáp là xong | khó | không thể tiếp cận |
| Sống bao lâu? | vài tuần/tháng | 1–2 năm | nhiều năm |
| Bao nhiêu người maintain? | 1 | 1–3 | nhiều, có người mới vào |
| Hỏng thì sao? | không sao | phiền | mất tiền / mất an toàn |

Đa số câu rơi vào cột nào thì là mức đó. Một câu "không thể tiếp cận" hoặc
"mất an toàn" tự động kéo lên production.

## Cái gì thuộc mức nào

| Hạng mục | Simple | Medium | Production |
|---|---|---|---|
| Application layer | `app_main` + vài hàm | file `app_*.c` riêng | component app, không chạm driver |
| Service layer | không | gộp vào app | tách, có interface |
| Driver layer | gọi thẳng `driver/*.h` | wrap mỗi ngoại vi 1 file | component có header hợp đồng |
| Hardware abstraction | không | không (trừ khi cần test host) | có, nếu đổi board/test host |
| Configuration | `#define` trong 1 header | `board_config.h` + Kconfig | Kconfig + NVS per-device |
| Communication | gọi hàm trực tiếp | queue giữa vài task | queue + hợp đồng rõ, không chia sẻ con trỏ sống |
| Event system | không | `esp_event` loop mặc định | event loop riêng, event ID có versioning |
| Task architecture | 1 task (`app_main`) | 2–4 task | task có chủ sở hữu tài nguyên rõ, watchdog từng task |
| State machine | `if/else` | `switch` + enum state | bảng chuyển trạng thái, log mọi transition |
| Error handling | `ESP_ERROR_CHECK` | trả `esp_err_t` lên, retry | phân loại fatal/retry/degrade, có fail-safe |
| Logging | `ESP_LOGI` | có TAG, có mức theo module | mức chỉnh runtime, log lỗi lưu lại được |
| Storage | không, hoặc NVS vài key | NVS có namespace | NVS + schema có version + migrate |
| OTA | không | thủ công | HTTPS OTA + ký + rollback + self-test |

## Quy tắc nâng mức

- Nâng mức khi có lý do cụ thể, không nâng "cho chắc".
- Nâng từng hạng mục được, không phải cả bảng. Một dự án simple vẫn có thể cần
  storage mức production nếu nó giữ dữ liệu hiệu chuẩn.
- Ghi lại **điều kiện kích hoạt** cho mục HOÃN: "thêm event bus khi có task thứ 4
  quan tâm sự kiện kết nối". Để lần sau có căn cứ, không phải bàn lại từ đầu.

## Dấu hiệu đang thừa kiến trúc

- Lớp chỉ chuyển tiếp lời gọi, không thêm gì (`svc_read()` chỉ gọi `drv_read()`).
- Interface chỉ có đúng một implementation và không có kế hoạch có cái thứ hai.
- Event bus mà mỗi event chỉ có một subscriber cố định.
- Queue giữa hai task luôn chạy đồng bộ với nhau — lẽ ra là một task.
- Nhiều file hơn số chức năng thật.
