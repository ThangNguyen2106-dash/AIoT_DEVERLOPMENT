# ESP32 Firmware Skills

Bộ Claude Code Skills cho phát triển firmware ESP32, dùng lại được cho nhiều dự án.

- Chip: ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6
- Framework: **ESP-IDF** (PlatformIO với `framework = espidf` là tương đương).
  Arduino core: các *nguyên tắc* vẫn áp dụng, nhưng *API và code mẫu thì không* —
  orchestrator sẽ nói rõ điều này thay vì im lặng đưa API không dùng được.
- Ưu tiên: reliability, maintainability, testability, security, hardware safety

Đây là **package nguồn** (source of truth). Sửa ở đây, rồi chạy installer để đồng bộ
sang nơi Claude Code đọc.

## Cài đặt

```powershell
# Windows — toàn cục, dùng được ở mọi dự án
.\install.ps1

# Windows — chỉ cho một dự án
.\install.ps1 -Scope Project -ProjectPath D:\work\my-esp32-app

# Gỡ
.\install.ps1 -Uninstall
```

```bash
# macOS / Linux / Git Bash
./install.sh                      # toàn cục
./install.sh /path/to/project     # riêng một dự án
./install.sh --uninstall
```

Installer idempotent: chạy lại bất cứ lúc nào để đồng bộ. Chỉ ghi đè thư mục `esp32-*`,
không đụng các skill khác. Sau khi cài, khởi động lại Claude Code.

> Không đặt junction/symlink từ `~/.claude/skills` vào đây: Node trả `isDirectory() = false`
> cho junction trên Windows, skill có thể không được nhận diện. Dùng installer (copy thật).

## Kiến trúc

Một orchestrator + 12 module chuyên sâu. Kiến thức nặng nằm trong `references/`,
chỉ được đọc khi thật sự chạm tới chủ đề.

```
skills/
  esp32-firmware/                   ← ENTRY POINT: 5 phase điều phối + cổng an toàn
    references/idf-versions.md          API vỡ theo phiên bản ESP-IDF
    checklists/pre-flash.md             cổng trước mọi lệnh flash/erase/burn
  esp32-01-project-init/            framework, cây thư mục, partition, Kconfig, dependency, CI
  esp32-02-hardware-analysis/       chọn chip, pin map, chân cấm, điện áp, ngân sách dòng
  esp32-03-firmware-architecture/   task, priority, ISR, phân lớp, lỗi, watchdog & crash recovery
  esp32-04-driver-development/      GPIO/I2C/SPI/UART/ADC/PWM + cảm biến, màn hình, actuator
  esp32-05-connectivity/            Wi-Fi, BLE, ESP-NOW, Ethernet, cellular, TCP/UDP,
                                    MQTT, HTTP(S), WebSocket, TLS, reconnect/offline
  esp32-06-application-development/ state machine, schema, NVS, filesystem, fail-safe
  esp32-07-debugging/               RCA 7 bước: panic, watchdog, heap, deadlock, boot fail
  esp32-08-testing/                 host test, Unity, pytest-embedded, QEMU, HIL, soak, CI
  esp32-09-performance-optimization/ RAM, stack, binary size, timing/jitter, throughput
  esp32-10-security/                bí mật, TLS, OTA ký, Secure Boot, eFuse, production
  esp32-11-power-management/        sleep, wake source, RTC state, ngân sách pin,
                                    bảng đánh đổi Power/Latency/Reliability/Functionality
  esp32-12-release/                 ship review, versioning, OTA, chẩn đoán từ xa, sản xuất
tools/
  lint.sh                           kiểm liên kết chết, tên skill, file mồ côi, ngân sách
```

### Orchestrator làm gì

5 phase, chạy theo thứ tự:

| Phase | Việc |
|---|---|
| 0 — Context Gate | dò chip / framework / phiên bản IDF / pinout / nguồn; thiếu thì **hỏi** |
| 1 — Phân loại task | quy yêu cầu về đúng một trong 12 loại; có bảng phân xử ranh giới hay nhầm |
| 2 — Routing + Prerequisite | chọn skill, và **kiểm điều kiện tiên quyết trước khi nạp** |
| 3 — Multi-skill | chọn skill chính trước, gọi skill phụ sau, có gói bàn giao 4 dòng |
| 4 — Clarification | phân biệt hỏi chặn với hỏi gom lô; câu hỏi phải thi hành được |
| 5 — Validation | đóng lượt bằng **mức kiểm chứng đã đạt**, không bằng "đã xong" |

## Cách dùng

Không cần gọi tên skill. Nhắc tới ESP32 / ESP-IDF / idf.py là `esp32-firmware` tự kích hoạt.

Muốn vào thẳng một module: `/esp32-07-debugging`, `/esp32-12-release`, …
Mỗi module 01–12 có mục **Điều kiện vào** nhắc chạy Context Gate nếu chưa rõ chip/framework —
nên vào thẳng cũng không mất cổng an toàn.

## Ngân sách context

Một lượt = orchestrator + **1** skill + **tối đa 1** reference. Không quét thư mục.

| | Kích thước |
|---|---|
| Orchestrator (nạp mỗi lượt ESP32) | ~13 KB |
| SKILL.md trung bình | 142 dòng |
| Reference lớn nhất | ~15.6 KB (trần lint: 16 KB) |
| Trần SKILL.md | 200 dòng (lint chặn) |
| Tổng `description` (nằm trong system prompt mọi phiên) | ~5.8 KB |

Trường hợp xấu nhất một lượt ≈ 42 KB ≈ 12k token. Đổi lại: không bao giờ nạp cả bộ knowledge.

## Nguyên tắc thiết kế

| Nguyên tắc | Cách hiện thực |
|---|---|
| Không lặp nội dung | Mỗi chủ đề có đúng một chủ sở hữu; mọi SKILL.md có mục "Không thuộc scope" trỏ tên skill cụ thể |
| Skill cấp cao chỉ điều phối | `esp32-firmware` không chứa kiến thức kỹ thuật, không sinh code |
| Không load toàn bộ knowledge | Chi tiết nằm ở `references/`, đọc theo yêu cầu, tối đa 1 file/lượt |
| Làm việc có quy trình | Context Gate + Prerequisite bắt buộc; thiếu thông tin thì hỏi, không đoán chip |
| Kết thúc bằng sự thật | Phase 5: báo mức kiểm chứng thật đã đạt, không suy diễn lên |
| Hardware safety | Cổng `pre-flash.md`; cấm tự chạy `espefuse burn_*`, `erase_flash`, bật Secure Boot |

### Luật phân xử khi hai skill nói khác nhau
1. Cổng an toàn phần cứng thắng tất cả.
2. `esp32-10-security` thắng ở mọi vấn đề bảo mật.
3. Còn lại: skill sở hữu chủ đề thắng (xem mục "Không thuộc scope" của từng skill).

## Cổng an toàn (bất biến toàn bộ package)

- Không chạy lệnh flash / erase / burn khi chưa qua `esp32-firmware/checklists/pre-flash.md`.
  Board có tải công suất đang gắn → hỏi xác nhận kể cả khi checklist đã qua.
- **Không bao giờ tự chạy** `espefuse.py burn_efuse` / `burn_key` — eFuse một chiều, vĩnh viễn.
- Không tự bật Secure Boot / Flash Encryption ở chế độ Release.
- `esptool erase_flash` xoá cả NVS (hiệu chuẩn, khoá, cấu hình) — phải xác nhận rõ ràng.
- Cơ cấu chấp hành phải ở trạng thái an toàn lúc boot và lúc panic.
- Brownout là lỗi nguồn — không đề xuất tắt brownout detector.

## Phát triển package

```bash
bash tools/lint.sh      # chạy trước mỗi lần commit
```

Quy ước viết skill mới hoặc sửa skill có sẵn: xem `CONTRIBUTING.md`.

## Phiên bản

Xem `VERSION` và `CHANGELOG.md`.
