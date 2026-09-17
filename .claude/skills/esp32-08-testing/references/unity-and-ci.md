# Unity trên target, pytest-embedded, và CI test

Build trong CI thuộc `esp32-01-project-init/references/ci.md`. File này lo phần **chạy test**.

## Ba cách chạy test, ba mục đích

| Cách | Chạy ở đâu | Dùng cho | Cần phần cứng |
|---|---|---|---|
| Host test | máy tính | logic thuần — phần lớn giá trị nằm ở đây | không |
| Unity trên target | chip thật / QEMU | driver, tích hợp với ESP-IDF | có (hoặc QEMU) |
| pytest-embedded | máy tính điều khiển chip | kịch bản nhiều bước, kiểm log, reset, HIL | có |

## Host test — bắt đầu từ đây

Chỉ chạy được khi logic đã tách khỏi I/O
(`esp32-06-application-development/references/state-machines.md`). Nếu chưa tách thì việc đầu
tiên không phải viết test — xem mục "Điều kiện tiên quyết" ở SKILL.md.

```
test/
  CMakeLists.txt        # dự án CMake thường, KHÔNG dùng ESP-IDF
  test_pump_logic.c
```

Biên dịch bằng `gcc`/`cmake` trên máy, chạy trong vài giây, chạy mỗi lần push.
Không cần framework nặng: Unity chạy được trên host, và `assert()` của C cũng đủ để bắt đầu.

## Unity trên target

```c
#include "unity.h"

TEST_CASE("sht3x chuyển raw sang độ C", "[sht3x]")
{
    TEST_ASSERT_EQUAL_FLOAT(25.0f, sht3x_raw_to_celsius(0x8000));
}

TEST_CASE("sht3x trả TIMEOUT khi thiết bị câm", "[sht3x][hw]")
{
    /* tag [hw] để lọc: chỉ chạy khi có phần cứng thật */
}
```

- Đặt trong `components/<name>/test/`, chạy bằng project trong `test/` với component `unity`.
- **Dùng tag** để tách test cần phần cứng (`[hw]`) khỏi test chạy được ở đâu cũng được.
  Không tách thì CI không có board sẽ đỏ vì lý do sai.
- Test trên target vẫn nên **nhỏ và độc lập**: mỗi test tự setup, tự dọn, không phụ thuộc
  thứ tự chạy.

## pytest-embedded

Cách chính thức hiện nay của Espressif để chạy test trên target một cách tự động — mạnh hơn
Unity ở chỗ nó điều khiển được cả **vòng đời thiết bị**: nạp firmware, reset, đọc log, gửi
dữ liệu vào UART, kiểm tra thiết bị boot lại đúng cách.

```python
def test_boot_va_ket_noi(dut):
    dut.expect('app_main started', timeout=10)
    dut.expect('wifi: connected', timeout=30)

def test_watchdog_bat_duoc_task_treo(dut):
    dut.write('hang_task\n')                 # lệnh debug ép một task treo
    dut.expect('Task watchdog got triggered', timeout=15)
    dut.expect('rst:0x')                     # thiết bị phải reset và boot lại
```

Đây là công cụ duy nhất trong ba cách kiểm được những thứ quan trọng nhất của firmware:
watchdog có thật sự bắt không, thiết bị có boot lại được không, core dump có sinh ra không.
Lưới an toàn ở `esp32-03-firmware-architecture/references/fault-tolerance.md` nên được kiểm
bằng cách này.

Cài: `pip install pytest-embedded pytest-embedded-idf pytest-embedded-serial-esp`.

## QEMU

`idf.py qemu` chạy firmware không phụ thuộc ngoại vi thật — hữu ích cho **smoke test**:
firmware boot được, init không abort, không panic trong vài giây đầu.

Giới hạn: không có ngoại vi thật, không có timing thật. Kết quả QEMU **không** được báo cáo
là mức `TARGET` — nó là mức `QEMU`, thấp hơn.

## Chạy test trong CI

```yaml
  host-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: cmake -S test -B test/build && cmake --build test/build && ctest --test-dir test/build --output-on-failure
```

Nguyên tắc:
- **Host test chạy mỗi lần push** và **chặn merge khi đỏ**. Test không chặn merge sẽ đỏ mãi mãi.
- Test cần phần cứng chỉ chạy trên runner có board gắn (self-hosted), hoặc chạy thủ công
  trước release. Đừng để chúng trong job chính của CI.
- Dùng Docker image `espressif/idf:v5.x` **cùng phiên bản** với máy dev — khác phiên bản thì
  CI xanh không nói lên điều gì về máy dev, và ngược lại.
- Test hồi quy cho một bug đã sửa phải **fail trên code cũ** trước khi được tính là chặn được
  bug đó (SKILL.md, quy trình bước 3).
