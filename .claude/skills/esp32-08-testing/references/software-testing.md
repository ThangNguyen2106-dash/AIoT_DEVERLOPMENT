# Tầng 2 — Software testing

Host test → unit test trên target → integration test → regression test.

## Test cái gì ở đâu

| Loại code | Test ở đâu | Vì sao |
|---|---|---|
| Chuyển đổi, lọc, CRC, parser, bảng trạng thái, tính toán | host | không cần chip, chạy mili giây, chạy được trong CI |
| Driver ngoại vi, NVS, timer, DMA | target (Unity) | phụ thuộc phần cứng thật, mock không chứng minh gì |
| Đường dữ liệu qua nhiều component | target (Unity) hoặc HIL | cần runtime FreeRTOS thật |
| Giao thức mạng, TLS | target + server thật/giả lập | timing và stack mạng thật |

Quy tắc: **cái gì test được ở host thì không đẩy lên target.** Target test chậm, cần
người cắm board, và hay flaky.

## Host test (logic thuần)

Điều kiện: file logic không include gì của ESP-IDF. Nếu chưa được như vậy →
`esp32-03-firmware-architecture`.

Cách tổ chức:

```
components/measure/
  include/measure.h
  measure.c            # thuần, không IDF
  test/test_measure.c  # Unity, chạy cả host lẫn target
host_test/             # project chạy trên máy (linux target của IDF, hoặc CMake thường)
```

ESP-IDF hỗ trợ `linux` target cho host test:

```bash
idf.py --preview set-target linux && idf.py build && ./build/host_test.elf
```

Test cái gì:
- Giá trị biên: 0, âm, giá trị lớn nhất, tràn.
- Đầu vào hỏng: CRC sai, gói ngắn, chuỗi không kết thúc, JSON sai cú pháp.
- Bảng chuyển trạng thái: mọi cặp (state, event), kể cả cặp "không hợp lệ".
- Tính đơn điệu/ổn định của bộ lọc khi đầu vào nhảy.

## Unit test trên target (Unity)

Xem cú pháp ở `unity-and-ci.md`. Nguyên tắc:

- Mỗi test tự dọn: cấu hình lại ngoại vi, xoá NVS key mình tạo. Không để test này
  phụ thuộc test kia — thứ tự chạy không đảm bảo.
- Test phải có timeout rõ ràng. Test treo còn tệ hơn test fail.
- Đánh tag theo yêu cầu phần cứng: `[sensor]`, `[needs_loopback]`, `[needs_wifi]`.
  Test cần dây nối ngoài phải ghi rõ nối gì vào đâu trong comment đầu file.
- Test driver không cần thiết bị ngoài: dùng loopback (UART TX→RX, SPI MOSI→MISO),
  timer, GPIO nối chéo hai chân.

## Integration test

Test **đường dữ liệu đi qua nhiều component**, chạy trên target với FreeRTOS thật.

Mẫu việc cần test:
- Cảm biến → service → queue → task gửi: giá trị đọc ra có tới được đầu bên kia đúng
  và đúng thứ tự không?
- Ghi cấu hình → reset → đọc lại: giá trị có sống qua reset không?
- Nhận lệnh từ mạng → đổi trạng thái → phản hồi: trọn vòng, đo được thời gian.
- Hai task tranh cùng một bus: chạy đồng thời N vòng, kiểm tính toàn vẹn dữ liệu.

Cách viết cho ổn định:
- Đồng bộ bằng queue/semaphore/event group, **không dùng `vTaskDelay` để "chờ cho chắc"** —
  đó là nguồn test flaky số một.
- Có timeout cho mọi lần chờ; timeout thì fail với thông báo nói rõ đang chờ cái gì.
- Kiểm cả free heap trước/sau để bắt rò rỉ sớm.

## Peripheral test và communication test

Ở mức phần mềm (trên target, chưa cần thiết bị đo) thì thuộc tầng này; phần cần máy đo
và thiết bị thật thuộc tầng 3 → `hardware-validation.md`.

- **Peripheral test**: init thành công, đọc/ghi được, xử lý đúng khi thiết bị không phản hồi,
  re-init sau lỗi có phục hồi không.
- **Communication test**: kết nối, gửi/nhận, mất kết nối giữa chừng, reconnect có backoff,
  dữ liệu trong hàng đợi khi offline. Chi tiết giao thức → `esp32-05-connectivity`.

## Regression test

**Mọi bug đã sửa phải kèm một test.** Quy trình bắt buộc:

1. Viết test tái hiện bug **trước khi sửa**. Nó phải **fail** trên code cũ.
   Không fail được → bạn chưa hiểu bug, hoặc test chưa chạm đúng chỗ.
2. Sửa code. Test chuyển sang pass.
3. Đặt test ở tầng thấp nhất có thể tái hiện được bug.
4. Comment đầu test ghi: triệu chứng, nguyên nhân gốc, ngày/issue. Người sau đọc test
   phải hiểu tại sao nó tồn tại, nếu không họ sẽ xoá nó.

```c
/* Regression: firmware treo khi gói UART dài 0 byte (2026-03, issue #142).
 * Nguyên nhân: vòng lặp parser giảm len chưa kiểm tra trước khi trừ → wrap-around. */
TEST_CASE("parser handles zero-length frame", "[parser]")
{
    TEST_ASSERT_EQUAL(PARSE_ERR_SHORT, parse_frame((uint8_t[]){0}, 0, &out));
}
```

Bug không tái hiện được bằng test (ví dụ race hiếm) → ít nhất thêm assert/log tại chỗ
và ghi vào test plan như mục cần soak test theo dõi.

## Bộ test phải giữ được

- Test flaky là nợ: sửa hoặc xoá, không để nó "thỉnh thoảng đỏ" — sau vài tuần
  không ai còn tin kết quả CI nữa.
- Test chậm tách thành nhóm chạy riêng (nightly), giữ nhóm nhanh chạy mỗi push.
- Test phải fail được. Thỉnh thoảng cố tình phá code để kiểm chứng test thực sự bắt.
