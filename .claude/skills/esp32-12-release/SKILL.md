---
name: esp32-12-release
description: Đưa firmware ESP32 ra thực địa — rà soát trước khi ship, đánh phiên bản, build production, OTA an toàn với esp_https_ota, rollback tự động và self-test sau cập nhật, chẩn đoán từ xa bằng core dump, quy trình nạp hàng loạt khi sản xuất. Dùng khi chuẩn bị release, khi triển khai cập nhật từ xa, hoặc khi cần biết firmware đã sẵn sàng ship chưa.
---

# 12 — Release

## Điều kiện vào
Chưa rõ chip / framework / phiên bản IDF → chạy Context Gate ở `esp32-firmware` trước.
Cổng an toàn và ngân sách context của `esp32-firmware` áp dụng nguyên vẹn tại đây.

Từ "code chạy trên bàn" đến "firmware chạy trên thiết bị của khách hàng **và cập nhật được**".
Đầu ra là một quyết định có căn cứ: ship được, hay còn N mục chặn.

## Nguyên tắc tối thượng: không có đường lùi thì không ship

Firmware ngoài thực địa không có người cắm cáp. Mọi thứ ở đây tồn tại để trả lời hai câu:
**bản này hỏng thì thiết bị tự về đâu?** và **bạn biết nó hỏng bằng cách nào?**

Không trả lời được cả hai → chưa sẵn sàng ship, dù tính năng đã đủ.

| Bị cấm | Vì sao |
|---|---|
| Bật OTA cho cả lô khi chưa thử trên nhóm nhỏ | một bản hỏng giết toàn bộ thiết bị cùng lúc |
| `esp_ota_mark_app_valid_cancel_rollback()` ở đầu `app_main` | vô hiệu hoá rollback, biến nó thành trang trí |
| `self_test_ok()` chỉ `return true` | như trên |
| Ship mà không ghi phiên bản vào binary và NVS | không truy được thiết bị nào đang chạy bản nào |
| Làm tròn "còn vài mục nhỏ" thành "sẵn sàng ship" | quyết định release là của user, phải dựa trên sự thật |
| Đổi partition table cho thiết bị đã ngoài thực địa | thiết bị cũ không nhận được — brick từ xa |

## Quy trình 7 bước

| # | Bước | Đầu ra | Reference |
|---|---|---|---|
| 1 | **Cổng vào** | mọi feature đã qua Definition of Done của `esp32-08-testing` | — |
| 2 | **Rà soát trước ship** | phát hiện theo 8 nhóm, kèm `file:line`, phân mức chặn/nên sửa/cải thiện | `ship-review.md` |
| 3 | **Phiên bản** | quy tắc đánh số, ghi vào binary **và** NVS, phân biệt bản dev | `versioning.md` |
| 4 | **Build production** | mức log, cờ debug, partition, cấu hình khác bản dev ở đâu | `versioning.md` + `esp32-10-security/references/production-config.md` |
| 5 | **OTA** | rollback bật, self-test thực chất, đã thử mất mạng và mất điện | `ota.md` |
| 6 | **Chẩn đoán từ xa** | core dump, reset reason, crash counter, phiên bản — đọc được mà không cần cáp | `remote-diagnostics.md` |
| 7 | **Sản xuất** | nạp hàng loạt, nạp danh tính riêng từng thiết bị, test xuất xưởng | `manufacturing.md` |

Bước 1 là **cổng cứng**: feature chưa qua DoD thì không đưa vào bản release.
Quan hệ hai checklist: **DoD = cổng cho từng feature; `ship-review.md` = cổng cho cả bản release**,
chạy trên toàn repo và giả định mọi feature đã qua DoD.

## Ba kịch bản OTA bắt buộc thử trước khi bật cho cả lô

Chưa chạy đủ ba cái này thì OTA là rủi ro, không phải tính năng:

1. **Mất mạng giữa chừng khi tải** → thiết bị vẫn boot vào bản cũ, thử lại được.
2. **Mất điện giữa chừng khi ghi** → bootloader vẫn chọn được slot hợp lệ.
3. **Bản mới tự test thất bại** → tự quay về bản cũ mà không cần can thiệp.

Triển khai theo lô: nhóm thử (vài thiết bị) → 10% → toàn bộ. Có đường **dừng phát hành** giữa chừng.

## Cổng an toàn trước mỗi lần nạp
Chạy `esp32-firmware/checklists/pre-flash.md`. Không tự chạy lệnh flash / erase / burn khi chưa qua.
Board có tải công suất đang gắn → hỏi xác nhận kể cả khi checklist đã qua.

## Mẫu báo cáo

```
## Phiên bản
<1.4.0> — build <ngày, commit> — target <esp32s3> — IDF <5.1.2>
Khác bản dev ở: <mức log, cờ debug, endpoint, partition>

## Rà soát 8 nhóm
| Nhóm | Phát hiện | Mức | file:line |
|---|---|---|---|
| 1 An toàn phần cứng | relay không có trạng thái an toàn lúc panic | CHẶN | main/relay.c:44 |
| 4 Đồng thời | biến chia sẻ không bảo vệ | NÊN SỬA | main/app.c:210 |

## Definition of Done
<feature nào đã qua, feature nào chưa, mục nào chưa đạt>

## OTA
Rollback: <bật/tắt>   Self-test kiểm: <những gì>
3 kịch bản: mất mạng <đã thử/chưa>  mất điện <...>  self-test fail <...>
Kế hoạch phát hành: <nhóm thử N thiết bị → 10% → toàn bộ; điều kiện dừng>

## Chẩn đoán từ xa
<core dump ở đâu, đọc bằng cách nào, ghi những gì vào NVS>

## Kết luận
<SẴN SÀNG SHIP / CÒN N MỤC CHẶN> — <liệt kê mục chặn, mỗi mục một dòng>
```

## References
- `references/ship-review.md` — 8 nhóm rà soát trước release
- `references/versioning.md` — đánh phiên bản, ghi vào binary và NVS, khác biệt build production
- `references/ota.md` — cơ chế OTA, rollback, self-test, chống brick
- `references/remote-diagnostics.md` — core dump, reset reason, crash counter, health log
- `references/manufacturing.md` — nạp hàng loạt, danh tính từng thiết bị, test xuất xưởng

## Không thuộc scope
- Bảo mật OTA (ký ảnh, anti-rollback, TLS verify), Secure Boot, Flash Encryption, eFuse
  → `esp32-10-security` (**thắng khi hai bên nói khác nhau**)
- Viết test tự động và Definition of Done → `esp32-08-testing`
- Điều tra một crash cụ thể → `esp32-07-debugging`
- Thiết kế watchdog, safe mode, bộ đếm crash → `esp32-03-firmware-architecture`
- Partition table và cấu hình build ban đầu → `esp32-01-project-init`
- Cửa sổ bảo dưỡng để OTA được khi thiết bị ngủ nhiều → `esp32-11-power-management`

## Đầu ra
Báo cáo theo mẫu + kết luận thẳng thắn về việc có ship được không. Còn mục CHẶN → nói rõ,
không làm tròn. Mức kiểm chứng tối thiểu cho một bản release: `SOAK`.
