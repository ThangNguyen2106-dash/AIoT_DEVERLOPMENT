# Changelog

## 2.0.0 — 2026-09-17

Viết lại orchestrator thành **ESP32 Development Orchestrator** đầy đủ, và sửa toàn bộ phát hiện
của lần review kiến trúc.

### Orchestrator `esp32-firmware`
- 5 phase: Context Gate → Phân loại task → Routing + **Prerequisite** → Multi-skill →
  Clarification → **Validation**
- Phase 1: bảng 12 loại task kèm tín hiệu nhận biết + bảng phân xử 10 ranh giới hay nhầm
  (07 vs 09 "hết RAM", 04 vs 07 bus im lặng, 06 vs 10 lưu NVS, 09 vs 11 …)
- Phase 2: mỗi skill có **prerequisite bắt buộc** và cách xử lý khi thiếu
- Phase 3: quy tắc chọn skill chính, gói **bàn giao 4 dòng** khi chuyển skill, luật phân xử
  xung đột (`esp32-10-security` thắng ở bảo mật; cổng an toàn phần cứng thắng tất cả)
- Phase 4: phân biệt **hỏi chặn** với hỏi gom lô; câu hỏi phải thi hành được
- Phase 5: đóng lượt bằng **mức kiểm chứng đã đạt** (BUILD→SOAK), có mức tối thiểu cho từng loại task
- Thêm `references/idf-versions.md` — bảng API vỡ theo phiên bản ESP-IDF

### Sửa lỗi
- **3 liên kết chết**: `esp32-project-setup/...`, `esp32-security/checklists/...`,
  và đường dẫn tương đối `references/chip-matrix.md` trong `pre-flash.md`
- **Context Gate bị bỏ qua** khi gọi thẳng `/esp32-NN-…`: thêm mục **Điều kiện vào** cho cả 12 skill
- **Mâu thuẫn `portMAX_DELAY`** giữa 07 (cấm) và 04/05/12 (bắt buộc bỏ) — phân biệt rõ
  "vá triệu chứng" với "thiết kế ở code mới"
- **Vòng lặp chết 08 ↔ 03**: 08 đẩy sang 03 để tách lớp, 03 từ chối refactor — thêm ngoại lệ
  tường minh ở 03 kèm phương án thay thế
- **Mâu thuẫn OTA prototype** giữa 10 (HTTP LAN chấp nhận được) và 12 (HTTPS tuyệt đối) —
  12 kế thừa mô hình 2 mức của 10
- **Biên dự phòng stack** 20% (09) vs 25% (08) → thống nhất 25%, con số chuẩn đặt ở `03/coding-rules.md`
- Gỡ trùng lặp code OTA giữa 10 và 12; tách ranh giới DoD (per-feature) vs ship-review (per-release)
- `04/actuators.md` được tuyên bố là **chủ sở hữu duy nhất** của quy tắc trạng thái an toàn actuator

### Nâng 4 skill từ chuẩn 1.0.0 lên chuẩn hiện hành
- **01 project-init**: quy trình 6 bước, 3 quyết định khó sửa sau khi ship, + `layout.md`,
  `kconfig.md`, `dependencies.md`, `ci.md`
- **06 application-development**: nguyên tắc "nghiệp vụ không chạm phần cứng", bảng ranh giới
  với driver, bảng fail-safe bắt buộc, + `state-machines.md`, `data-schema.md`, `filesystem.md`,
  `fail-safe.md`
- **09 performance**: "không có số thì không sửa", thứ tự ưu tiên tối ưu, danh sách cấm,
  + `measure.md`, `memory.md`, `binary-size.md`, `timing.md`, `throughput.md`
- **12 release**: quy trình 7 bước, 3 kịch bản OTA bắt buộc, + `versioning.md`,
  `remote-diagnostics.md`, `manufacturing.md`

### Bổ sung nội dung
- **`03/fault-tolerance.md`** (mới) — watchdog TWDT/IWDT/RTC, trạng thái an toàn khi panic,
  bộ đếm crash + safe mode, factory reset. Trước đây không skill nào sở hữu chủ đề này;
  hạng mục thứ **14** của skill 03
- `03/coding-rules.md`: quy tắc tràn bộ đếm `uint32_t` ms (49 ngày), biên stack chuẩn
- `05/connection-contract.md`: bảng ánh xạ từ vựng lỗi driver (04) ↔ 5 lớp lỗi mạng (05)
- `08/unity-and-ci.md`: viết lại, thêm **pytest-embedded**, tách 3 cách chạy test

### Scope và ngân sách
- **Arduino**: thu scope thay vì hứa suông — orchestrator nói rõ bộ skill viết cho ESP-IDF,
  nguyên tắc nào vẫn dùng được, API nào không
- Rút gọn 7 description: tổng 7.063 → 5.841 ký tự (chi phí cố định ở mọi phiên)
- README cập nhật theo số liệu thật (trước đây ghi "SKILL.md ~35 dòng", thực tế 142)

### Công cụ
- **`tools/lint.sh`** — kiểm frontmatter, liên kết nội bộ, liên kết chéo skill, tên skill cũ,
  file mồ côi, ngân sách context (SKILL.md ≤ 200 dòng, reference ≤ 16 KB)
- **`CONTRIBUTING.md`** — khung SKILL.md bắt buộc, quy tắc nội dung, cách viết description,
  luật phân xử xung đột, quy trình thêm/sửa skill

## 1.6.0 — 2026-09-17
- Viết lại `esp32-11-power-management` theo chuẩn production, cấm đánh đổi âm thầm:
  luật bắt buộc công bố bảng 4 trục **Power / Latency / Reliability / Functionality** cho mọi
  đề xuất tiết kiệm điện, danh sách những đánh đổi không được tự ý làm (tắt brownout/watchdog,
  bỏ TLS, bỏ retry/buffer, giãn chu kỳ đo dưới mức nghiệp vụ, ngủ khi actuator đang bật)
- Quy trình 8 bước bắt đầu bằng ngân sách năng lượng; hợp đồng module pm với khoá chống ngủ
  (OTA, ghi flash, đang gửi, actuator) và cửa sổ bảo dưỡng để còn OTA được ngoài hiện trường
- Thêm 7 reference: `power-budget.md`, `modes.md`, `wake-sources.md`, `deep-sleep-state.md`,
  `peripheral-power.md`, `wifi-power.md`, `tradeoffs.md`; bỏ `sleep-and-budget.md`
- Thêm `checklists/power-review.md`
- Cập nhật bảng định tuyến trong orchestrator `esp32-firmware` và README

## 1.5.0 — 2026-09-17
- Mở rộng `esp32-10-security` cho firmware production: cảnh báo eFuse một chiều,
  nguyên tắc không hard-code bí mật (password, API key, private key, token, production
  credential), bảng phân biệt prototype và production cho 10 hạng mục, 8 lớp phòng thủ
  theo thứ tự, quy trình rà soát 8 bước, mẫu báo cáo có phân mức CHẶN/CẢNH BÁO/THÔNG TIN
- Thêm 6 reference cho 10: `secrets.md`, `tls-certificates.md`, `ota-security.md`,
  `secure-boot-flash-encryption.md`, `debug-interfaces.md`, `production-config.md`
- Viết lại `hardening.md` thành checklist hai cột P / PROD, có định tuyến sang từng reference
- Cập nhật bảng định tuyến trong orchestrator `esp32-firmware`

## 1.4.0 — 2026-09-16
- Viết lại `esp32-07-debugging` theo hướng root-cause analysis, cấm workaround che triệu chứng:
  quy trình 7 bước Symptom → Evidence → Hypothesis → Verification → Root Cause → Minimal Fix →
  Regression Test, thang bằng chứng 5 hạng, danh sách 10 hành vi bị cấm (delay, tăng stack,
  nới/tắt watchdog, restart định kỳ, nuốt esp_err_t, tắt brownout), bảng định tuyến 17 triệu chứng
- Thêm 2 checklist: `evidence-request.md` (bằng chứng cần cho 10 lớp lỗi + câu hỏi mẫu khi thiếu
  log, yêu cầu hỏi theo lô thay vì đoán), `rca-checklist.md` (7 cổng + cổng an toàn)
- Thay `references/diagnostics.md` bằng 5 reference chuyên lớp lỗi: `boot-and-reset.md`,
  `panic-backtrace.md`, `memory-faults.md`, `rtos-faults.md`, `comm-timeouts.md`;
  viết lại `tooling.md`
- Thêm `templates/rca-report.md`

## 1.3.0 — 2026-09-16
- Viết lại `esp32-05-connectivity` theo chuẩn production, cấm code mạng chỉ có happy path:
  7 câu hỏi bắt buộc, phân loại lỗi 5 lớp (INIT / LINK / AUTH / PROTO / SERVER) kèm chính
  sách riêng cho từng lớp, quy trình 9 bước, hợp đồng state machine `net_state_t`, mẫu báo cáo
- Mở rộng scope: thêm Ethernet (EMAC nội + W5500 SPI), modem cellular ngoài (esp_modem/PPPoS),
  TCP/UDP socket thuần, WebSocket; tách MQTT và HTTP thành hai reference riêng
- Thêm 8 reference: `connection-contract.md` (state/timeout/backoff/offline/buffering/counter),
  `ethernet.md`, `cellular.md`, `mqtt.md`, `http.md`, `websocket.md`, `tcp-udp.md`, `tls.md`;
  mở rộng `wifi.md` và `ble-espnow.md`; bỏ `mqtt-http.md`
- Thêm `checklists/connectivity-checklist.md` gồm cả mục kiểm thử đường lỗi
- Cập nhật bảng định tuyến trong orchestrator `esp32-firmware` và README

## 1.2.0 — 2026-09-16
- Mở rộng `esp32-04-driver-development`: quy trình 10 bước (Requirement → Interface →
  Initialization → Configuration → Read/Write → Timeout → Error handling → Recovery →
  Logging → Test), 6 điều bắt buộc với mọi driver, hợp đồng API handle-based, mẫu báo cáo
- Thêm 7 reference cho 04: `driver-contract.md`, `gpio.md`, `rs485-modbus.md`,
  `twai-can.md`, `sensors.md`, `displays.md`, `actuators.md`
- Cập nhật bảng định tuyến trong orchestrator `esp32-firmware`

## 1.1.0 — 2026-09-16
- Xây dựng đầy đủ `esp32-02-hardware-analysis` theo tiêu chuẩn phát triển sản phẩm:
  quy trình 4 bước, thang bằng chứng 5 hạng, 10 hạng mục kiểm tra bắt buộc, mẫu báo cáo
  có cấu trúc với 3 mức CHẶN/CẢNH BÁO/THÔNG TIN
- Thêm 5 reference: pin-constraints, peripheral-matrix, boot-debug-usb, flash-psram,
  power-electrical; mở rộng chip-matrix
- Thêm `checklists/analysis-checklist.md` và `templates/pin-map.md`

## 1.0.0 — 2026-09-16
- Bộ khởi tạo: 1 orchestrator + 12 module (01-project-init .. 12-release)
- 25 file reference/template/checklist
- Installer PowerShell + Bash, cài toàn cục hoặc theo dự án
