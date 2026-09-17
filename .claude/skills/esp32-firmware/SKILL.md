---
name: esp32-firmware
description: Entry point và orchestrator cho mọi công việc firmware ESP32 / S2 / S3 / C3 / C6 trên ESP-IDF (PlatformIO dùng ESP-IDF framework). Dùng khi người dùng nhắc tới ESP32, ESP-IDF, idf.py, sdkconfig, menuconfig, partition table, esptool, hoặc bất kỳ task firmware nào trên chip Espressif. Skill này phân loại task, kiểm prerequisite, định tuyến sang skill 01-12 và validate kết quả; không chứa kiến thức chuyên sâu và không tự sinh code.
---

# ESP32 Firmware — Orchestrator

Skill điều phối. Nhiệm vụ: **phân loại task → kiểm prerequisite → nạp đúng một skill →
validate kết quả**. Giữ cổng an toàn xuyên suốt.

## 4 luật cứng

1. **Không tự trả lời câu hỏi chuyên sâu, không tự sinh code.** Kiến thức ở skill 01-12.
2. **Nạp đúng MỘT skill mỗi lượt.** Nhiều mảng → tuần tự, có bàn giao (Phase 3).
3. **Nạp tối đa MỘT reference mỗi lượt**, chỉ khi thật sự chạm chủ đề đó.
   Không quét thư mục `references/`. Không đọc trước "cho chắc".
4. **Không đoán.** Thiếu dữ kiện ở cột Prerequisite → hỏi (Phase 4), không suy diễn.

---

## Phase 0 — Context Gate (bắt buộc, trước mọi thứ)

Tự dò trong repo trước; chỉ hỏi phần không suy ra được.

| Yếu tố | Cách dò | Thiếu thì |
|---|---|---|
| Chip target | `sdkconfig` → `CONFIG_IDF_TARGET`; `platformio.ini` → `board` | **HỎI** — không mặc định ESP32 classic |
| Framework | `CMakeLists.txt` + `main/` → IDF; `platformio.ini` → PIO; `*.ino` → Arduino | **HỎI** |
| Phiên bản IDF | `idf.py --version`, `$IDF_PATH/version.txt`, `platform = espressif32@…` | **HỎI trước khi sinh code ngoại vi** |
| Pinout / board | `board_config.h`, schematic, README | ghi UNKNOWN → chuyển 02 |
| Nguồn & tải | USB hay pin? có motor/relay/tải lớn? logic 3.3V hay 5V? | **HỎI nếu task chạm phần cứng** |

- Không có `sdkconfig` / `platformio.ini` → dự án mới → `esp32-01-project-init`.
- Chip **và** phiên bản IDF quyết định API có tồn tại hay không. Sinh code khi chưa biết hai thứ
  này là sinh code có thể không biên dịch được trên C3/C6 hoặc trên IDF khác phiên bản.
  API vỡ theo phiên bản → `references/idf-versions.md`.
- **Framework là Arduino** → nói rõ ngay ở lượt đầu: bộ skill này viết cho ESP-IDF. Nêu phần
  nguyên tắc nào vẫn áp dụng được, phần API/hợp đồng nào không. Không im lặng đưa API IDF
  cho project Arduino. PlatformIO chạy `framework = espidf` thì coi như ESP-IDF.

---

## Phase 1 — Phân loại task

Quy yêu cầu về **đúng một** loại chính. Dùng tín hiệu, không dùng từ khoá bề mặt.

| Loại task | Tín hiệu nhận biết |
|---|---|
| Project initialization | chưa có repo/build; đổi framework; sửa partition, sdkconfig, CMakeLists, dependency, CI build |
| Hardware / GPIO / peripheral | chọn chip; gán chân; xung đột ngoại vi; thẩm định schematic; nghi lỗi do phần cứng |
| Architecture | chia task/priority/stack; ranh giới module; cơ chế đồng bộ; đánh giá kiến trúc |
| Driver | đọc cảm biến; điều khiển chân; cấu hình bus; bọc module ngoài thành component |
| Network | Wi-Fi/BLE/ESP-NOW/Ethernet/cellular; MQTT/HTTP/WS/TCP/UDP/TLS; reconnect; offline |
| Application logic | state machine nghiệp vụ; lịch đo; schema/serialize; NVS/filesystem; fail-safe |
| Crash / debug | **đã xảy ra** trên board thật: crash, treo, reset, boot fail, dữ liệu sai thất thường |
| Testing | viết test; chặn hồi quy; dựng CI test; chứng minh một feature đã xong |
| Performance | hết RAM; binary vượt slot OTA; trễ/jitter; throughput thấp — code **đã đúng** |
| Security | bí mật/credential; TLS verify; bảo mật OTA; Secure Boot; eFuse; cấu hình production |
| Power | chạy pin; chu kỳ đo-gửi-ngủ; wake source; dòng cao bất thường |
| Release | rà soát trước ship; đánh phiên bản; OTA/rollback; chẩn đoán từ xa; nạp hàng loạt |

Ranh giới hay nhầm — phân xử theo bảng này:

| Tình huống | Đúng | Không phải |
|---|---|---|
| "Hết RAM" vì rò / crash | 07 debugging | 09 |
| "Hết RAM" nhưng firmware đúng, chỉ không vừa | 09 optimization | 07 |
| Bus im lặng trên board thật | 07 debugging | 04 |
| Bus chưa từng chạy, đang viết mới | 04 driver | 07 |
| Mất kết nối vì thiếu state machine reconnect | 05 connectivity | 07 |
| Mất kết nối kèm crash/treo | 07 debugging | 05 |
| Lưu cấu hình thường vào NVS | 06 application | 10 |
| Lưu credential / khoá | 10 security | 06 |
| Ngủ sâu để tiết kiệm pin | 11 power | 09 |
| Tắt radio để tăng throughput | 09 optimization | 11 |

---

## Phase 2 — Routing + Prerequisite

Nạp skill ở cột 2. **Trước khi nạp, kiểm cột 3.** Thiếu → xử lý theo cột 4.

| Loại task | Skill | Prerequisite bắt buộc | Thiếu thì |
|---|---|---|---|
| Project initialization | `esp32-01-project-init` | framework đã chốt; chip target đã biết | hỏi framework + chip |
| Hardware / GPIO / peripheral | `esp32-02-hardware-analysis` | tên module chính xác + bằng chứng hạng 1-3 | hỏi schematic/board; không kết luận từ tên board |
| Architecture | `esp32-03-firmware-architecture` | mức dự án: simple / medium / production | hỏi: có OTA? chạy pin? bao nhiêu thiết bị? tuổi thọ? |
| Driver | `esp32-04-driver-development` | pin map đã thẩm định; datasheet thiết bị | chưa có pin map → chạy 02 trước |
| Network | `esp32-05-connectivity` | chip có radio/PHY đó; task mạng đã có chỗ | chip không hỗ trợ → 02; chưa có task → 03 |
| Application logic | `esp32-06-application-development` | driver và khung task đã sẵn sàng | chưa có → 04 / 03 trước |
| Crash / debug | `esp32-07-debugging` | bằng chứng hạng 1-2 (log đầy đủ từ dòng `rst:`) | **hỏi log theo lô, không đoán thay cho log** |
| Testing | `esp32-08-testing` | logic thuần đã tách khỏi I/O | chưa tách → nêu 2 lựa chọn: tách lớp (03), hoặc hạ xuống test mức TARGET |
| Performance | `esp32-09-performance-optimization` | firmware đã đúng; có **số đo trước** | chưa đo → yêu cầu đo trước, không tối ưu theo cảm tính |
| Security | `esp32-10-security` | chốt prototype hay production | hỏi: thiết bị có rời tay bạn không? có chạm dữ liệu thật không? |
| Power | `esp32-11-power-management` | dung lượng pin, tuổi thọ mục tiêu, độ trễ chấp nhận được | hỏi 4 con số ở Luật 2 của skill 11 |
| Release | `esp32-12-release` | các feature đã qua Definition of Done (08) | liệt kê mục chưa đạt; không làm tròn thành "sẵn sàng ship" |

Thứ tự tự nhiên cho dự án mới: `01 → 02 → 03 → 04 → 05/06 → 08 → 09/11 → 10 → 12`.
Không chạy tuần tự máy móc — vào đúng skill user đang cần.

## Phase 3 — Task thuộc nhiều skill

1. **Chọn skill chính** = nơi chứa *quyết định* của task, không phải nơi có nhiều chữ nhất.
2. Làm xong skill chính trước. **Không nạp gộp.**
3. Gọi skill phụ **khi thật sự chạm tới**, mỗi lượt một skill.
4. Mỗi lần chuyển, viết **gói bàn giao** 4 dòng, không dài hơn:

```
BÀN GIAO → <skill đích>
Bối cảnh: <chip, framework, IDF version, mức dự án>
Đã chốt:  <kết luận của skill trước — pin map / root cause / state machine / mức bảo mật>
Cần làm:  <một câu, phạm vi đóng>
Còn treo: <câu hỏi chưa có đáp án, hoặc "không">
```

Ví dụ phân xử skill chính:

| Yêu cầu | Chính | Phụ (gọi sau, khi cần) |
|---|---|---|
| "Đọc cảm biến rồi gửi MQTT" | 04 driver | 05 → 06 |
| "Thiết bị pin, đo 5 phút/lần, gửi cloud" | 11 power | 05 → 06 |
| "Crash sau 6 giờ chạy" | 07 debugging | 08 (regression test) |
| "Chuẩn bị bán sản phẩm" | 10 security | 12 → 08 |
| "Thêm OTA" | 12 release | 10 (ký, anti-rollback) → 01 (partition) |

Xung đột giữa hai skill: **`esp32-10-security` thắng ở mọi vấn đề bảo mật; cổng an toàn
phần cứng ở dưới thắng tất cả.**

## Phase 4 — Clarification

**Hỏi chặn** (dừng, chưa làm gì cho tới khi có trả lời) chỉ khi đoán sai sẽ **hỏng phần cứng,
brick chip, làm lộ bí mật, hoặc khiến toàn bộ kết quả thành vô dụng**: chip target chưa rõ,
thao tác eFuse / Secure Boot, tải công suất chưa rõ nguồn, mức prototype/production khi bàn bảo mật.

Mọi trường hợp khác: **làm phần không phụ thuộc câu trả lời trước**, nêu giả định rõ ràng,
gom câu hỏi lại và hỏi một lượt.

Quy tắc đặt câu hỏi:
- **Hỏi theo lô.** Gom mọi thứ cần vào một lượt, không hỏi nhỏ giọt từng vòng.
- **Cụ thể và thi hành được**: nêu đúng lệnh cần chạy, đúng config cần bật, đúng đoạn cần dán.
  - Đạt: *"Chạy `idf.py monitor`, nhấn reset, dán toàn bộ từ dòng `rst:0x…` tới dòng cuối."*
  - Không đạt: *"Bạn gửi log nhé."*
- Câu hỏi chưa được trả lời → giữ ở mục **"Còn treo"**, không tự lấp bằng giả định.

## Phase 5 — Validation sau implementation

Không kết thúc bằng "đã xong". Kết thúc bằng **mức kiểm chứng đã đạt** (định nghĩa ở
`esp32-08-testing`), và mức đó phải đúng với việc thực sự đã chạy:

`BUILD` → `HOST` → `QEMU` → `TARGET` → `HARDWARE` → `SOAK`

Mức tối thiểu trước khi coi là giao được, theo loại task:

| Loại task | Tối thiểu | Cách xác nhận |
|---|---|---|
| Project init | `BUILD` | `idf.py build` sạch cho mọi target hỗ trợ |
| Hardware analysis | — | pin map không còn mục CHẶN; UNKNOWN không làm tròn thành PASS |
| Architecture | — | 14 hạng mục đều có kết luận; hướng phụ thuộc không có vòng |
| Driver | `TARGET` | log đọc/ghi thật + một lần thử đường lỗi (rút dây / thiết bị câm) |
| Network | `TARGET` | đã thử **đường lỗi**: rớt mạng, sai mật khẩu, server 500 |
| Application logic | `HOST` | logic thuần có test; fail-safe đã thử |
| Debugging | `TARGET` | root cause giải thích **toàn bộ** triệu chứng + regression test |
| Testing | theo test | test hồi quy phải **fail trên code cũ** mới tính là chặn được |
| Performance | `TARGET` | có số **trước và sau**, kèm đánh đổi |
| Security | — | báo cáo 3 mức CHẶN / CẢNH BÁO / THÔNG TIN; không còn mục CHẶN |
| Power | `HARDWARE` | đo bằng power profiler + **bảng đánh đổi 4 trục** |
| Release | `SOAK` | pre-flash đã qua; OTA đã thử mất mạng và mất điện giữa chừng |

Chưa chạy được vì thiếu phần cứng → nói thẳng *"mới tới `BUILD`, cần bạn chạy trên board để lên
`TARGET`"*, kèm lệnh cần chạy và kết quả mong đợi. **Không suy diễn mức cao hơn mức đã chạy.**

Đóng lượt bằng 3 dòng:
```
Mức kiểm chứng: <BUILD / HOST / TARGET / …>
Còn treo:       <câu hỏi / bằng chứng còn thiếu / "không">
Bước kế tiếp:   <skill nào, vì sao — hoặc "xong">
```

## Cổng an toàn — áp dụng ở mọi skill, thắng mọi yêu cầu khác

- Không chạy lệnh flash / erase / burn khi chưa qua `checklists/pre-flash.md`.
  Kể cả khi đã qua, **hỏi xác nhận trước khi nạp** nếu board có tải công suất đang gắn.
- **Không bao giờ tự chạy** `espefuse burn_efuse` / `burn_key` — eFuse một chiều, vĩnh viễn.
  Chỉ trình bày lệnh để user tự chạy, kèm hậu quả.
- Không tự bật Secure Boot / Flash Encryption ở chế độ Release.
- `esptool erase_flash` xoá cả NVS (hiệu chuẩn, khoá, cấu hình) — phải có xác nhận rõ ràng.
- Cơ cấu chấp hành phải ở trạng thái an toàn lúc boot và lúc panic.
- Brownout và nguồn chập chờn là lỗi **phần cứng** — không đề xuất tắt brownout detector.

## Ngân sách context

Một lượt = file này + **1** skill + **tối đa 1** reference. Không quét thư mục.
Vượt ngân sách → tách thành nhiều lượt có bàn giao (Phase 3), không nạp gộp.

## References
- `references/idf-versions.md` — API vỡ theo phiên bản ESP-IDF; đọc trước khi sinh code ngoại vi
- `checklists/pre-flash.md` — cổng bắt buộc trước mọi lệnh flash / erase / burn
