// #define DEBUG_COLOR
#include <Arduino.h>
#include <secrets.h>
#include <AIoT.h>
#define SENSOR_1 36
#define SENSOR_2 39
#define SENSOR_3 34
#define SENSOR_4 35
#define OUT1 32
#define OUT2 33
#define OUT3 25
#define OUT4 26
#define BUTTON1 23
#define BUTTON2 5
#define BUTTON3 13
int8_t SENSOR[4] = {SENSOR_1, SENSOR_2, SENSOR_3, SENSOR_4};
int8_t OUT[4] = {OUT1, OUT2, OUT3, OUT4};
int8_t BUTTON[3] = {BUTTON1, BUTTON2, BUTTON3};
float rawTemp = 0.0f;
float rawHum = 0.0f;
float rawCO2 = 0.0f;

int8_t mode = 0;
bool lastButtonState = HIGH;
bool stableButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
void handleButton()
{
    bool reading = digitalRead(BUTTON1);

    // Phát hiện trạng thái nút thay đổi
    if (reading != lastButtonState)
    {
        lastDebounceTime = millis();
    }
    // Nếu trạng thái đã ổn định đủ lâu
    if ((millis() - lastDebounceTime) > debounceDelay)
    {
        if (reading != stableButtonState)
        {
            stableButtonState = reading;
            if (stableButtonState == HIGH)
            {
                mode++;
                if (mode > 3)
                    mode = 0;
                Serial.println("Chế độ:" + String(mode));
            }
        }
    }
    lastButtonState = reading;
}

void setup()
{
    Serial.begin(115200);
    for (int i = 0; i < 4; i++)
    {
        pinMode(SENSOR[i], INPUT);
        pinMode(OUT[i], OUTPUT);
        analogWrite(OUT[i], 0);
    }
    for (int i = 0; i < 3; i++)
    {
        pinMode(BUTTON[i], INPUT);
    }
    AIoT.begin("", "", "", "");
    AIoT.device.begin();
    AIoT.edgeAI.begin(12, 3);
}

// Giả lập hệ thống hoạt động bình thường
void NormamlState()
{
    float rawTemp = 26.0f + (float(random(0.0, 10.0) / 10.0));
    float rawHum = 60.0f + (float(random(0.0, 100.0) / 100.0)) + (float(random(0.0, 100.0) / 1000.0));
    float rawCO2 = 600.0f + (float(random(0.0, 400.0))) + float(random(0.0, 100.0) / 1000.0);

    float cleadTemp = AIoT.edgeAI.push(0, rawTemp);
    float cleanHum = AIoT.edgeAI.push(1, rawHum);
    float cleanCO2 = AIoT.edgeAI.push(2, rawCO2);

    if (AIoT.edgeAI.isReady())
    {
        AIoT.edgeAI.predict();
        const float *feats = AIoT.edgeAI.getRawFeatures();
        for (int i = 0; i < 12; i++)
        {
            Serial.printf("%.2f,", feats[i]);
        }
        Serial.println("LABEL:" + String(mode));
    }
}

void loop()
{
    handleButton();
    switch (mode)
    {
    case 0:
        NormamlState();
        delay(1000);
        break;
    }
}