# Middleware Layer - AIoT_LIB

Phân hệ **Middleware** cung cấp các cổng kết nối và giao thức trung gian đưa dữ liệu, quyết định và trạng thái từ **Hybrid AI** tới các hệ thống giám sát và điều khiển ngoại vi.

## Các Giao Thức Được Hỗ Trợ

- **MQTT over TLS:** Giao tiếp xuất/nhập bản tin Telemetry và Downlink qua cổng bảo mật 8883 (HiveMQ Cloud / EMQX / Mosquitto).
- **HTTP / WebServer:** Cổng Captive Portal nội bộ (cổng 80) phục vụ cấu hình tham số mạng WiFi và broker khi thiết bị ngoại tuyến.
- **Serial / Modbus:** Cổng truyền thông công nghiệp phục vụ đọc dữ liệu PLC hoặc giao tiếp máy tính nhúng.
- **ROS 2 Integration Boundary:** Điểm giao tiếp biên tích hợp với Robot Operating System (ROS 2) qua micro-ROS hoặc cầu nối serial/MQTT. Thiết kế theo ranh giới độc lập (Integration Boundary) đảm bảo ROS 2 không trở thành thư viện phụ thuộc cứng (hard dependency) của nhân Hybrid AI.
