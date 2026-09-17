# Schema dữ liệu và serialize

## Luật: mọi cấu trúc dữ liệu rời khỏi thiết bị đều phải có version

Thiết bị ngoài thực địa **không** cập nhật cùng lúc với server. Sẽ luôn có giai đoạn
firmware cũ nói chuyện với server mới, và ngược lại. Không có trường version thì giai đoạn đó
là giai đoạn hỏng.

Áp dụng cho: telemetry gửi lên, lệnh nhận xuống, cấu hình lưu trong NVS, file trên filesystem.

```json
{ "v": 2, "ts": 1737100800, "dev": "a1b2c3", "t": 24.7, "h": 61.2 }
```

`v` là version **của schema**, không phải của firmware. Nó chỉ tăng khi cấu trúc đổi.

## Quy tắc tương thích ngược

| Thay đổi | Có phá tương thích? | Cần tăng version? |
|---|---|---|
| Thêm trường **tuỳ chọn** | không | không |
| Thêm trường **bắt buộc** | có | **có** |
| Đổi tên trường | có | **có** |
| Đổi đơn vị (mV → V) | **có, và âm thầm** | **có** — loại nguy hiểm nhất |
| Đổi kiểu (int → float, string → number) | có | **có** |
| Bỏ trường | có, với bên đọc cũ | **có** |

Đổi đơn vị mà không đổi tên trường và không tăng version là lỗi kinh điển: không ai thấy lỗi,
dữ liệu chỉ đơn giản sai 1000 lần.

Bên nhận phải: **bỏ qua trường không biết** (không lỗi), và **từ chối rõ ràng** version lớn hơn
version cao nhất nó hiểu (không đoán).

## JSON hay nhị phân

| | JSON | Nhị phân (struct đóng gói / CBOR / protobuf) |
|---|---|---|
| Kích thước | 3-10× lớn hơn | nhỏ nhất |
| RAM khi parse | 1.5-3× payload | gần bằng payload |
| Gỡ lỗi | đọc được bằng mắt | cần công cụ |
| Đổi schema | dễ | phải quản lý chặt |
| Hợp với | cấu hình, lệnh, telemetry thưa | telemetry tần suất cao, thiết bị pin, băng thông hẹp |

Khuyến nghị: **JSON là mặc định**; chuyển sang nhị phân chỉ khi có số chứng minh (băng thông,
RAM, hoặc điện năng — `esp32-11-power-management`). Đừng tối ưu định dạng trước khi đo.

## Nếu dùng nhị phân

```c
typedef struct __attribute__((packed)) {
    uint8_t  version;      /* LUÔN là trường đầu tiên */
    uint8_t  type;
    uint16_t len;          /* độ dài phần payload theo sau */
    uint32_t ts;
    int16_t  temp_cdeg;    /* 24.7 °C → 2470, tránh float */
} telemetry_hdr_t;
```

- **`version` là byte đầu tiên**, luôn luôn — bên nhận phải biết cách đọc trước khi đọc.
- `__attribute__((packed))` để tránh padding khác nhau giữa trình biên dịch.
- Ghi rõ **endianness** trong tài liệu schema. ESP32 là little-endian; server có thể không.
- Dùng số nguyên có hệ số (centi-degree) thay cho float: nhỏ hơn, không có vấn đề làm tròn,
  và tránh float trong đường nóng.
- **Không bao giờ** `memcpy` thẳng một struct từ gói tin nhận được vào bộ nhớ rồi dùng —
  phải kiểm `len`, kiểm dải từng trường. Đây là mặt tấn công → `esp32-10-security`.

## Parse JSON trên ESP32

- `cJSON` có sẵn trong IDF, dễ dùng, nhưng cấp phát nhiều mảnh nhỏ → phân mảnh heap nếu gọi
  liên tục (`esp32-09/references/memory.md`). Parse xong **giải phóng ngay**, không giữ cây JSON.
- Payload lớn: parse theo stream thay vì nạp cả chuỗi vào RAM.
- **Luôn kiểm tra `NULL`** cho mỗi trường lấy ra, và kiểm kiểu trước khi đọc giá trị.
  Gói tin từ mạng không bao giờ được giả định là đúng định dạng.
- Giới hạn kích thước payload chấp nhận **trước khi** cấp phát, không sau.

## Schema của cấu hình lưu trong NVS

Cấu hình cũng cần version, vì OTA có thể đổi cấu trúc cấu hình:

```c
typedef struct {
    uint16_t schema_ver;
    uint16_t crc;          /* phát hiện hỏng, không chỉ phát hiện thiếu */
    ...
} app_config_t;
```

Khi đọc: `schema_ver` nhỏ hơn hiện tại → **migrate** (điền mặc định cho trường mới, chuyển đổi
trường đổi đơn vị) rồi ghi lại. Lớn hơn → thiết bị vừa bị hạ cấp firmware: dùng mặc định an toàn,
log W, **không** cố đọc. Chi tiết lưu trữ: `storage-nvs.md`.

## Tài liệu schema

Giữ một file mô tả schema trong repo (không chỉ trong code): tên trường, kiểu, đơn vị, dải hợp lệ,
bắt buộc/tuỳ chọn, version xuất hiện. Đây là hợp đồng với phía server — và là thứ đầu tiên
cần đến khi dữ liệu trông sai.
