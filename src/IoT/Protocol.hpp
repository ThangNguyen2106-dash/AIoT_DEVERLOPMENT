#ifndef INC_AIOT_PROTOCOL_HPP_
#define INC_AIOT_PROTOCOL_HPP_

#include <IoT/DEBUG.hpp>
#include <WiFi/AIoT_PnP_ESP32.hpp>
#include <HybridAI/HybridAI.h>
#include <Device/Device.h>

class AIoTProtocol
{
private:
    PnP<MQTTESP32<PubSubClient>> PNP;
    API API_MESS;
    cJSON *tele_root = NULL;
    cJSON *dataObj_tele = NULL;
    cJSON *control_root = NULL;
    cJSON *dataObj_control = NULL;
    unsigned long IoT_time, IoT_set_time;

public:
    HybridAIEngine hybridAI;
    EdgeAI::Engine &edgeAI;
    CloudAI::GeminiClient &cloudAI;
    AIoTDeviceManager &device;

    AIoTProtocol();
    ~AIoTProtocol();
    void begin(const char *sta_ssid, const char *sta_pass);
    void begin(const char *sta_ssid, const char *sta_pass, const char *mqtt_id, const char *mqtt_auth);
    void run();

    bool CheckConnect();

    template <typename... Args>
    void setTelemetry(Args... args);
    template <typename... Args>
    void setControl(Args... args);

    void updateTelemetry(const char *key, const Param value);
    void updateControl(const char *key, const Param value);
    void sendTelemetry();
    void sendControl();
    void writeControl(const char *key, const Param value);
    void writeTelemetry(const char *key, const Param value);
    int addTimeEvent(unsigned long time, void (*callback)());
    void timeEvented();
    void (*_timerCallback)() = NULL;
};

AIoTProtocol::AIoTProtocol(/* args */)
    : edgeAI(hybridAI.edge),
      cloudAI(hybridAI.gemini),
      device(AIoT_Device)
{
}

AIoTProtocol::~AIoTProtocol()
{
}

void AIoTProtocol::begin(const char *sta_ssid, const char *sta_pass)
{
    this->PNP.begin(sta_ssid, sta_pass);
}
void AIoTProtocol::begin(const char *sta_ssid, const char *sta_pass, const char *mqtt_userName, const char *mqtt_pass)
{
    this->PNP.begin(sta_ssid, sta_pass, mqtt_userName, mqtt_pass);
}

void AIoTProtocol::timeEvented()
{
    unsigned long now = millis();

    if (now - IoT_time >= IoT_set_time)
    {
        IoT_time = now;

        if (_timerCallback != NULL)
        {
            _timerCallback();
        }
    }
}

int AIoTProtocol::addTimeEvent(unsigned long time, void (*callback)())
{
    IoT_set_time = time;
    IoT_time = millis();
    _timerCallback = callback;
    return 1;
}

template <typename... Args>
void AIoTProtocol::setControl(Args... args)
{
    if (control_root == NULL)
    {
        control_root = cJSON_CreateObject();
        dataObj_control = cJSON_CreateObject();

        char macStr[18];
        WiFi.macAddress().toCharArray(macStr, sizeof(macStr));
        cJSON_AddStringToObject(control_root, "mac_address", macStr);
        cJSON_AddItemToObject(control_root, "data", dataObj_control);
    }
    else
    {
        cJSON_DeleteItemFromObject(control_root, "data");
        dataObj_control = cJSON_CreateObject();
        cJSON_AddItemToObject(control_root, "data", dataObj_control);
    }

    const char *keys[] = {args...};

    constexpr size_t count = sizeof...(args);

    for (size_t i = 0; i < count; i++)
    {
        if (keys[i] == nullptr)
            continue;

        cJSON_AddNumberToObject(
            dataObj_control,
            keys[i],
            0);
    }

    char buffer[256];

    if (cJSON_PrintPreallocated(control_root, buffer, sizeof(buffer), 0))
    {
        LOG_DEBUG("SET_TELE", "%s", buffer);

        API_MESS.Set_control(buffer);
    }
    else
    {
        LOG_ERROR("SET_TELE", "Buffer too small!");
    }
}

template <typename... Args>
void AIoTProtocol::setTelemetry(Args... args)
{
    if (tele_root == NULL)
    {
        tele_root = cJSON_CreateObject();
        dataObj_tele = cJSON_CreateObject();

        char macStr[18];
        WiFi.macAddress().toCharArray(macStr, sizeof(macStr));
        cJSON_AddStringToObject(tele_root, "mac_address", macStr);
        cJSON_AddItemToObject(tele_root, "data", dataObj_tele);
    }
    else
    {
        cJSON_DeleteItemFromObject(tele_root, "data");
        dataObj_tele = cJSON_CreateObject();
        cJSON_AddItemToObject(tele_root, "data", dataObj_tele);
    }

    const char *keys[] = {args...};

    constexpr size_t count = sizeof...(args);

    for (size_t i = 0; i < count; i++)
    {
        if (keys[i] == nullptr)
            continue;

        cJSON_AddNumberToObject(dataObj_tele, keys[i], 0);
    }

    char buffer[256];

    if (cJSON_PrintPreallocated(tele_root, buffer, sizeof(buffer), 0))
    {
        LOG_DEBUG("SET_TELE", "%s", buffer);
        API_MESS.Set_telemetry(buffer);
    }
    else
    {
        LOG_ERROR("SET_TELE", "Buffer too small!");
    }
}

void AIoTProtocol::updateTelemetry(const char *key, const Param value)
{
    this->API_MESS.SetTelemetryValue(key, value);
}

void AIoTProtocol::sendTelemetry()
{
    if ((WiFi.status() == WL_CONNECTED) && serverMQTT.check_connect())
    {
        const char *data = this->API_MESS.GetTelemetryJson();
        if (data != nullptr)
        {
            serverMQTT.PublishData_tele(data);
        }
    }
}

void AIoTProtocol::updateControl(const char *key, const Param value)
{
    this->API_MESS.WriteControl(key, value);
}

void AIoTProtocol::sendControl()
{
    if ((WiFi.status() == WL_CONNECTED) && serverMQTT.check_connect())
    {
        // Gửi control hiện tại
    }
}

void AIoTProtocol::writeControl(const char *key, const Param value)
{
    if ((WiFi.status() == WL_CONNECTED) && serverMQTT.check_connect())
    {
        const char *data_control = this->API_MESS.WriteControl(key, value);
        if (data_control != nullptr)
        {
            serverMQTT.PublishData_control(data_control);
        }
    }
}

void AIoTProtocol::writeTelemetry(const char *key, const Param value)
{
    if ((WiFi.status() == WL_CONNECTED) && serverMQTT.check_connect())
    {
        const char *data = this->API_MESS.WriteTelemetry(key, value);
        if (data != nullptr)
        {
            serverMQTT.PublishData_tele(data);
        }
    }
}

bool AIoTProtocol::CheckConnect()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void AIoTProtocol::run()
{
    this->PNP.run();
    this->timeEvented();
}

typedef AIoTProtocol PROTOCOL;
AIoTProtocol AIoT;

#endif /*INC_AIOT_PROTOCOL_HPP_*/