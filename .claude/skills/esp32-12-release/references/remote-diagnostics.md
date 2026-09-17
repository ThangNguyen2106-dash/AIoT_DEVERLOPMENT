# Chẩn đoán từ xa

Thiết bị ngoài thực địa crash lúc 3 giờ sáng, không ai cắm cáp. Cái bạn chuẩn bị **trước khi ship**
là toàn bộ những gì bạn sẽ có để điều tra. Chuẩn bị thiếu = sự cố không bao giờ tìm ra nguyên nhân.

## Bốn thứ tối thiểu phải có

| # | Thứ | Vì sao |
|---|---|---|
| 1 | Phiên bản firmware | không biết bản nào thì mọi phân tích là đoán (`versioning.md`) |
| 2 | Lý do reset gần nhất | phân biệt crash / watchdog / brownout / mất điện — khoanh vùng ngay lập tức |
| 3 | Bộ đếm crash và số lần boot | phân biệt sự cố một lần với boot loop |
| 4 | Health log định kỳ | thấy xu hướng (rò heap) trước khi thành crash |

## Lý do reset

```c
switch (esp_reset_reason()) {
case ESP_RST_POWERON:  /* mất điện / cắm lại   */ break;
case ESP_RST_SW:       /* esp_restart() có chủ đích */ break;
case ESP_RST_PANIC:    /* crash — có core dump không? */ break;
case ESP_RST_TASK_WDT: /* task treo   */ break;
case ESP_RST_INT_WDT:  /* ISR/critical section quá dài */ break;
case ESP_RST_BROWNOUT: /* LỖI NGUỒN — phần cứng, không phải firmware */ break;
case ESP_RST_DEEPSLEEP:/* bình thường nếu thiết bị chạy pin */ break;
default: break;
}
```

Ghi vào NVS **ngay đầu `app_main`**, trước khi thứ gì đó có thể crash lần nữa.
Đếm riêng từng loại: `panic_count`, `wdt_count`, `brownout_count`. Một thiết bị có
`brownout_count` tăng đều là vấn đề nguồn, đừng tìm bug trong code.

Ý nghĩa từng lý do và cách điều tra: `esp32-07-debugging/references/boot-and-reset.md`.

## Core dump vào flash

Không có core dump thì một vụ panic ngoài thực địa chỉ để lại "nó reset".

```
CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y
CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF=y
CONFIG_ESP_COREDUMP_CHECKSUM_CRC32=y
```

Cần một partition `coredump` trong `partitions.csv`
(`esp32-01-project-init/references/partitions.md`) — thiếu partition thì cờ trên vô nghĩa.

Sau khi boot lại, kiểm tra và đẩy về server:

```c
esp_core_dump_summary_t s;
if (esp_core_dump_get_summary(&s) == ESP_OK) {
    /* s.exc_pc, s.exc_task, s.bt_info — đủ để khoanh vùng mà không cần tải cả dump */
    report_crash(&s);                 /* gửi lên server ở lần online kế tiếp */
    esp_core_dump_image_erase();      /* xoá sau khi đã gửi thành công, không trước */
}
```

- **Chỉ xoá sau khi gửi thành công.** Xoá trước là mất bằng chứng duy nhất.
- Giải mã cần **đúng ELF của bản firmware đó** — đây là lý do phải lưu ELF theo từng tag.
  ELF sai → tên hàm sai → điều tra đi sai hướng (`esp32-07-debugging`).
- Core dump chứa nội dung RAM, có thể gồm cả bí mật. Ở production, cân nhắc rủi ro lộ dữ liệu
  và đường truyền phải mã hoá → `esp32-10-security/references/debug-interfaces.md`.

## Health log định kỳ

Mỗi 60 s (mức INFO), và gửi lên server mỗi lần telemetry:

```
uptime=3612s heap_min=38912 largest=12288 rst=PANIC(3) wifi_drops=2 mqtt_drops=1 fw=1.4.0
```

- `heap_min` giảm dần qua nhiều giờ = rò bộ nhớ, thấy được **trước** khi thiết bị crash.
- Counter mạng lấy từ `net_get_stats()` của `esp32-05-connectivity` — đừng dựng bộ đếm song song.
- Giữ một dòng, cùng định dạng, dễ parse bằng máy. Log đẹp cho người đọc thì khó thống kê.

## Mức log ở production

Quá nhiều log: nghẽn UART, ảnh hưởng timing (`esp32-09/references/timing.md`), tốn băng thông.
Quá ít: sự cố không truy được.

Mức đúng: **INFO cho mốc quyết định và mọi lỗi; DEBUG tắt**. Ba nguyên tắc:
- Log ở **mốc chuyển trạng thái**, không log ở mỗi vòng lặp.
- Mỗi lỗi log **một lần, ở một lớp** — không log lại ở mọi tầng gọi lên.
- **Không bao giờ log bí mật**, kể cả DEBUG, kể cả tạm thời (`esp32-10-security`).

Cân nhắc một cờ điều khiển từ xa để tạm nâng mức log của **một** module trên **một** thiết bị
đang có vấn đề — rẻ hơn rất nhiều so với gửi người tới hiện trường.
