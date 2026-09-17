# Các khoá sdkconfig hay phải đụng tới

## Cơ bản
```
CONFIG_IDF_TARGET="esp32s3"
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_COMPILER_OPTIMIZATION_SIZE=y          # -Os, mặc định tốt cho production
CONFIG_COMPILER_WARN_WRITE_STRINGS=y
```

## Log
```
CONFIG_LOG_DEFAULT_LEVEL_INFO=y              # production
CONFIG_LOG_MAXIMUM_LEVEL_DEBUG=y             # cho phép nâng level lúc runtime
```

## FreeRTOS & watchdog
```
CONFIG_FREERTOS_HZ=1000                      # tick 1ms nếu cần timing mịn (tốn CPU hơn)
CONFIG_ESP_TASK_WDT_EN=y
CONFIG_ESP_TASK_WDT_TIMEOUT_S=5
CONFIG_ESP_INT_WDT=y
CONFIG_FREERTOS_USE_TRACE_FACILITY=y         # cần cho vTaskGetRunTimeStats
CONFIG_FREERTOS_CHECK_STACKOVERFLOW_CANARY=y
```

## Debug / chẩn đoán sự cố
```
CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y
CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF=y
CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT=y       # production
CONFIG_ESP_SYSTEM_PANIC_GDBSTUB=y            # chỉ khi debug tại bàn
CONFIG_HEAP_POISONING_LIGHT=y                # bật khi đang truy bug heap
```

## Brownout — đừng tắt để "cho hết reset"
```
CONFIG_ESP_BROWNOUT_DET=y
CONFIG_ESP_BROWNOUT_DET_LVL_SEL_7=y
```
Brownout reset là triệu chứng nguồn yếu, không phải lỗi phần mềm. Sửa nguồn, đừng tắt cảnh báo.

## PSRAM (S2/S3 có PSRAM)
```
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384
```
Lưu ý: buffer DMA thường KHÔNG đặt được ở PSRAM trên mọi ngoại vi — kiểm tra trước.
