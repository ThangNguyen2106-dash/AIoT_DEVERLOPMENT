/**
 * ============================================================================
 * AIoT_LIB - Ví Dụ 10: Giao Tiếp Công Nghiệp RS485 Modbus RTU
 * ============================================================================
 * Chức năng:
 * - Khởi tạo giao tiếp Modbus RTU Master qua cổng phần cứng Serial2
 * - Đọc thanh ghi Holding Registers từ cảm biến Modbus công nghiệp (Slave ID: 1)
 * ============================================================================
 */

#include <Arduino.h>

#define DEBUG_COLOR
#define BUTTON_CONFIG

#include <AIoT.h>

// Định nghĩa chân giao tiếp module phần cứng RS485
#define PIN_RS485_RX 16 // Chân RX của module RS485 nối với ESP32
#define PIN_RS485_TX 17 // Chân TX của module RS485 nối với ESP32
#define MODBUS_BAUDRATE 9600

float humidity = 0.0f;
float temperature = 0.0f;

void readModbusSensor()
{
    uint16_t buffer[2];

    // Đọc 2 thanh ghi từ địa chỉ 0x0000 của thiết bị Slave ID 1
    int result = TZModbus.readHoldingRegisterValue(1, 0x0000, 2, buffer);

    if (result > 0)
    {
        humidity = (float)buffer[0] / 10.0f;
        temperature = (float)buffer[1] / 10.0f;

        Serial.printf("[MODBUS] Do am: %.1f %% | Nhiet do: %.1f *C\n", humidity, temperature);
    }
    else
    {
        Serial.printf("[MODBUS ERROR] Loi doc thanh ghi: Ma loi %d\n", result);
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== AIoT_LIB: Vi Du 10 - RS485 Modbus RTU ===");

    // Khởi tạo cổng truyền thông Modbus RTU
    TZModbus.beginModbus(Serial2, MODBUS_BAUDRATE, PIN_RS485_RX, PIN_RS485_TX, SERIAL_8N1);
    Serial.println("Modbus Master da khoi dong xong.");
}

void loop()
{
    readModbusSensor();
    delay(2000);
}
