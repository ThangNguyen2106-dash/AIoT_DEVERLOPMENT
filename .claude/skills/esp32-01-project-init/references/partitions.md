# Partition table

## Mẫu không OTA, flash 4MB
```csv
# Name,   Type, SubType, Offset,  Size,    Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x300000,
storage,  data, spiffs,  ,        0xF0000,
```

## Mẫu có OTA, flash 4MB
```csv
# Name,   Type, SubType, Offset,  Size,    Flags
nvs,      data, nvs,     0x9000,  0x4000,
otadata,  data, ota,     0xd000,  0x2000,
phy_init, data, phy,     0xf000,  0x1000,
ota_0,    app,  ota_0,   0x10000, 0x180000,
ota_1,    app,  ota_1,   ,        0x180000,
nvs_keys, data, nvs_keys,,        0x1000,  encrypted
```

## Quy tắc
- Partition `app` phải căn chỉnh 0x10000 (64KB). Partition `data` căn chỉnh 0x1000.
- OTA cần cả `otadata` lẫn 2 slot `ota_X` kích thước BẰNG NHAU.
- Đã có `factory` + OTA thì bootloader chạy `factory` khi `otadata` trống.
- Bật Flash Encryption: NVS không tự mã hoá, cần partition `nvs_keys` và bật NVS encryption.
- Flash 4MB thực tế còn ~3.9MB dùng được sau bootloader + bảng partition.
- Đổi layout partition trên thiết bị đã triển khai sẽ làm mất dữ liệu NVS → cần kế hoạch migrate.

## Kiểm tra
```
idf.py partition-table
idf.py size            # tổng quan
idf.py size-components # xem component nào ngốn flash
```
