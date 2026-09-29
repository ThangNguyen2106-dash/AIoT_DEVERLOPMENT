/* How to read Payload Json

*/
#ifndef INC_API_HPP_
#define INC_API_HPP_

#include <IoT/Topic.h>
#include <Ultility/Param/cJSON.hpp>
#include <IoT/Param.hpp>
#include <IoT/Handler.hpp>

DataHandler AIoT_Data_Handler;
#define TECH AIoT_Data_Handler

typedef enum
{
    PIN_DO,
    PIN_DI,
    PIN_AI,
    PIN_AO,
    PIN_UNKNOWN
} PinType_t;

inline PinType_t parsePinType(const char *type)
{
    if (strcmp(type, "DO") == 0)
        return PIN_DO;
    if (strcmp(type, "DI") == 0)
        return PIN_DI;
    if (strcmp(type, "AI") == 0)
        return PIN_AI;
    if (strcmp(type, "AO") == 0)
        return PIN_AO;
    return PIN_UNKNOWN;
}

class AIoT_API
{

public:
    void begin() {}
    void handleMessage(const char *topic, const char *payload);
    void dowm(const char *payload);
    void handlerVirtual_Pin(const char *payload);
    void handler_control(const char *payload);
    void handlerArduino_Pin(const char *payload);
    void handler_data(const char *payload);
    void virtualWrite(uint16_t pin, const Param &param);
    void Set_telemetry(const char *telemetry);
    void Set_control(const char *control);
    void SetTelemetryValue(const char *key, const Param value);
    const char *GetTelemetryJson();
    const char *WriteControl(const char *key, const Param value);
    const char *WriteTelemetry(const char *key, const Param value);

private:
    int Pin;
    const char *type;
    const char *type_value;
    uint8_t V_pin;
    cJSON *telemetry_root = NULL;
    cJSON *control_root = NULL;

#define API_SUB_PREFIX_CONTROL_TOPIC "control"
#ifdef API_100_PINS
    String uuid_list[100];
    int16_t ID_control[100];
#else
    String uuid_list[50];
    int16_t ID_control[50];
#endif
};

typedef AIoT_API API;

void AIoT_API::handleMessage(const char *topic, const char *payload)
{
    if (strncmp(topic, BASE_TOPIC, strlen(BASE_TOPIC)) != 0)
        return;

    String topicStr = topic;
    String sub_Prefix = "";
    int p1 = topicStr.indexOf('/');
    if (p1 != -1)
    {
        int p2 = topicStr.indexOf('/', p1 + 1);
        if (p2 != -1)
        {
            int p3 = topicStr.indexOf('/', p2 + 1);
            if (p3 != -1)
            {
                sub_Prefix = topicStr.substring(p2 + 1, p3);
            }
            else
            {
                sub_Prefix = topicStr.substring(p2 + 1);
            }
        }
    }

    if (sub_Prefix == API_SUB_PREFIX_CONTROL_TOPIC)
    {
        this->handler_control(payload);
    }
}

void API::handler_control(const char *payload)
{
    cJSON *root = cJSON_Parse(payload);
    if (root == NULL)
    {
        LOG_ERROR("API", "JSON PARSE FAILED!\n");
        return;
    }

    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (cJSON_IsObject(data))
    {
        // Object Type
        cJSON *Object = cJSON_GetObjectItem(data, "V_pin");
        if (cJSON_IsObject(Object))
        {
            char *Object_tem = cJSON_Print(Object);
            if (Object_tem != nullptr)
            {
                this->handlerVirtual_Pin(Object_tem);
                free(Object_tem);
                Object_tem = nullptr;
            }
        }
        cJSON *Object1 = cJSON_GetObjectItem(data, "mcu_pin");
        if (cJSON_IsObject(Object1))
        {
            char *Object_tem1 = cJSON_Print(data);
            if (Object_tem1 != nullptr)
            {
                this->handlerArduino_Pin(Object_tem1);
                free(Object_tem1);
                Object_tem1 = nullptr;
            }
        }
        else if (cJSON_IsArray(Object1))
        {
            char *Object_tem1 = cJSON_Print(data);
            if (Object_tem1 != nullptr)
            {
                this->handlerArduino_Pin(Object_tem1);
                free(Object_tem1);
                Object_tem1 = nullptr;
            }
        }

        cJSON *item = data->child;
        while (item)
        {
            const char *key =
                item->string;

            Param param =
                parseItem(item);

            TECH.dispatch(key, param);

            item = item->next;
        }
    }
    cJSON *uuid = cJSON_GetObjectItem(root, "uuid");
    if (cJSON_IsString(uuid))
    {
        uuid_list[V_pin] = uuid->valuestring;
    }

    cJSON *control_id = cJSON_GetObjectItem(root, "control_id");
    if (cJSON_IsNumber(control_id))
    {
        ID_control[V_pin] = control_id->valueint;
    }
    cJSON_Delete(root);
}

void API::handlerArduino_Pin(const char *payload)
{

    cJSON *root = cJSON_Parse(payload);
    if (root == NULL)
    {
        LOG_ERROR("API", "JSON PARSE FAILED!\n");
        return;
    }
    cJSON *arr = cJSON_GetObjectItem(root, "mcu_pin");
    if (cJSON_IsArray(arr))
    {
        int size = cJSON_GetArraySize(arr);

        for (int i = 0; i < size; i++)
        {
            cJSON *obj = cJSON_GetArrayItem(arr, i);
            if (!cJSON_IsObject(obj))
                continue;

            // ===== GPIO =====
            cJSON *GPIO = cJSON_GetObjectItem(obj, "GPIO");
            if (cJSON_IsNumber(GPIO))
            {

                Pin = GPIO->valueint;
            }
            // ===== Type ====
            cJSON *Pin_type = cJSON_GetObjectItem(obj, "Pintype");
            if (cJSON_IsString(Pin_type))
            {

                type = Pin_type->valuestring;
                //"pinType":DO/DI/AI/AO,
            }
            // ===== value ====
            cJSON *handle_value = cJSON_GetObjectItem(obj, "value");
            Param val = parseItem(handle_value);

            switch (parsePinType(type))
            {
            case PIN_DO:
                pinMode(Pin, OUTPUT);
                digitalWrite(Pin, val.getInt());
                break;
            case PIN_DI:
                pinMode(Pin, INPUT);
                break;
            case PIN_AI:
                pinMode(Pin, INPUT);
                break;
            case PIN_AO:
                pinMode(Pin, OUTPUT);
                digitalWrite(Pin, val.getInt());
                break;
            default:
                break;
            }
        }
    }
    else
    {
        // ===== GPIO =====
        cJSON *GPIO = cJSON_GetObjectItem(arr, "GPIO");
        if (cJSON_IsNumber(GPIO))
        {

            Pin = GPIO->valueint;
        }
        // ===== Type ====
        cJSON *Pin_type = cJSON_GetObjectItem(arr, "Pintype");
        if (cJSON_IsString(Pin_type))
        {

            type = Pin_type->valuestring;
            //"pinType":DO/DI/AI/AO,
        }
        // ===== value ====
        cJSON *handle_value = cJSON_GetObjectItem(arr, "value");
        Param val = parseItem(handle_value);

        switch (parsePinType(type))
        {
        case PIN_DO:
            pinMode(Pin, OUTPUT);
            digitalWrite(Pin, val.getInt());
            break;
        case PIN_DI:
            pinMode(Pin, INPUT);
            break;
        case PIN_AI:
            pinMode(Pin, INPUT);
            break;
        case PIN_AO:
            pinMode(Pin, OUTPUT);
            digitalWrite(Pin, val.getInt());
            break;
        default:
            break;
        }
    }
    cJSON_Delete(root);
}

void API::handlerVirtual_Pin(const char *payload)
{
    cJSON *root = cJSON_Parse(payload);
    if (root == NULL)
    {
        LOG_ERROR("API", "JSON PARSE FAILED!\n");
        return;
    }

    // ===== Type ====
    cJSON *Pin_type = cJSON_GetObjectItem(root, "pin");
    if (cJSON_IsString(Pin_type) && Pin_type->valuestring != NULL)
    {

        type = Pin_type->valuestring;
        if (type[0] == 'V')
        {
            V_pin = atoi(type + 1);
        }
    }

    // ===== value ====
    cJSON *handle_value = cJSON_GetObjectItem(root, "value");
    Param val = parseItem(handle_value);

    // ===== DISPATCH =====
    if (V_pin < HandlerCount)
    {
        HandlerVector[V_pin](V_pin, val);
    }
    else
    {
    }
    cJSON_Delete(root);
}

void API::dowm(const char *payload)
{
    cJSON *root = cJSON_Parse(payload);
    if (root == NULL)
    {

        return;
    }

    cJSON *arr = cJSON_GetObjectItem(root, "virtual_pins");

    if (cJSON_IsArray(arr))
    {
        int size = cJSON_GetArraySize(arr);

        for (int i = 0; i < size; i++)
        {
            cJSON *obj = cJSON_GetArrayItem(arr, i);
            if (!cJSON_IsObject(obj))
                continue;

            // ===== id =====
            cJSON *id = cJSON_GetObjectItem(obj, "id");
            if (cJSON_IsNumber(id))
            {
            }

            // ===== config =====
            cJSON *config = cJSON_GetObjectItem(obj, "config");
            if (cJSON_IsNumber(config))
            {
            }

            // ===== pinType =====
            cJSON *type = cJSON_GetObjectItem(obj, "pinType");
            if (cJSON_IsString(type))
            {
            }
        }
    }
    else
    {
        LOG_WARN("API", "virtual_pins is not array");
    }

    cJSON_Delete(root);
}

void API::Set_telemetry(const char *telemetry)
{
    if (telemetry_root != NULL)
    {
        cJSON_Delete(telemetry_root);
        telemetry_root = NULL;
    }

    telemetry_root = cJSON_Parse(telemetry);
    char *jsonStr = cJSON_PrintUnformatted(telemetry_root);

    if (telemetry_root == NULL)
    {
        LOG_ERROR("API", "JSON PARSE FAILED!\n");
        return;
    }
}

void API::Set_control(const char *control)
{
    if (control_root != NULL)
    {
        cJSON_Delete(control_root);
        control_root = NULL;
    }

    control_root = cJSON_Parse(control);
    char *jsonStr = cJSON_PrintUnformatted(control_root);

    if (control_root == NULL)
    {
        LOG_ERROR("API", "JSON PARSE FAILED!\n");
        return;
    }
}

const char *API::WriteControl(const char *key, const Param value)
{
    if (!control_root)
    {
        control_root = cJSON_CreateObject();
        char macStr[18];
        WiFi.macAddress().toCharArray(macStr, sizeof(macStr));
        cJSON_AddStringToObject(control_root, "mac_address", macStr);
        cJSON_AddItemToObject(control_root, "data", cJSON_CreateObject());
    }

    cJSON *ObjectData = cJSON_GetObjectItem(control_root, "data");
    if (!ObjectData)
    {
        ObjectData = cJSON_CreateObject();
        cJSON_AddItemToObject(control_root, "data", ObjectData);
    }

    cJSON *newItem = NULL;

    switch (value.getType())
    {
    case Param::Type::INT:
        newItem = cJSON_CreateNumber(value.getInt());
        break;

    case Param::Type::FLOAT:
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", value.getFloat());
        newItem = cJSON_CreateRaw(buf);
        break;
    }

    case Param::Type::DOUBLE:
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", value.getDouble());
        newItem = cJSON_CreateRaw(buf);
        break;
    }

    case Param::Type::BOOL:
        newItem = cJSON_CreateBool(value.getBool());
        break;

    case Param::Type::STRING:
        newItem = cJSON_CreateString(value.getString().c_str());
        break;

    default:
        return nullptr;
    }

    if (!newItem)
        return nullptr;

    cJSON *existing = cJSON_GetObjectItem(ObjectData, key);

    if (existing)
        cJSON_ReplaceItemInObject(ObjectData, key, newItem);
    else
        cJSON_AddItemToObject(ObjectData, key, newItem);

    static char buffer[512];

    if (cJSON_PrintPreallocated(control_root, buffer, sizeof(buffer), 0))
    {
        return buffer;
    }
    else
    {
        LOG_ERROR("CONTROL", "Buffer too small!");
        return nullptr;
    }
}

void API::SetTelemetryValue(const char *key, const Param value)
{
    if (!telemetry_root)
    {
        telemetry_root = cJSON_CreateObject();
        char macStr[18];
        WiFi.macAddress().toCharArray(macStr, sizeof(macStr));
        cJSON_AddStringToObject(telemetry_root, "mac_address", macStr);
        cJSON_AddItemToObject(telemetry_root, "data", cJSON_CreateObject());
    }

    cJSON *ObjectData = cJSON_GetObjectItem(telemetry_root, "data");
    if (!ObjectData)
    {
        ObjectData = cJSON_CreateObject();
        cJSON_AddItemToObject(telemetry_root, "data", ObjectData);
    }

    cJSON *newItem = NULL;

    switch (value.getType())
    {
    case Param::Type::INT:
        newItem = cJSON_CreateNumber(value.getInt());
        break;

    case Param::Type::FLOAT:
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", value.getFloat());
        newItem = cJSON_CreateRaw(buf);
        break;
    }

    case Param::Type::DOUBLE:
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", value.getDouble());
        newItem = cJSON_CreateRaw(buf);
        break;
    }

    case Param::Type::BOOL:
        newItem = cJSON_CreateBool(value.getBool());
        break;

    case Param::Type::STRING:
        newItem = cJSON_CreateString(value.getString().c_str());
        break;

    default:
        return;
    }

    if (!newItem)
        return;

    cJSON *existing = cJSON_GetObjectItem(ObjectData, key);
    if (existing)
        cJSON_ReplaceItemInObject(ObjectData, key, newItem);
    else
        cJSON_AddItemToObject(ObjectData, key, newItem);
}

const char *API::GetTelemetryJson()
{
    if (!telemetry_root)
        return nullptr;

    static char buffer[512];
    if (cJSON_PrintPreallocated(telemetry_root, buffer, sizeof(buffer), 0))
    {
        return buffer;
    }
    else
    {
        LOG_ERROR("TELEMETRY", "Buffer too small!");
        return nullptr;
    }
}

const char *API::WriteTelemetry(const char *key, const Param value)
{
    SetTelemetryValue(key, value);
    return GetTelemetryJson();
}

void API::handler_data(const char *payload)
{
    cJSON *root =
        cJSON_Parse(payload);

    if (root == NULL)
        return;

    cJSON *data =
        cJSON_GetObjectItem(
            root,
            "data");

    if (cJSON_IsObject(data))
    {
        cJSON *item =
            data->child;

        while (item)
        {
            const char *key =
                item->string;

            Param param =
                parseItem(item);

            TECH.dispatch(key, param);

            item = item->next;
        }
    }

    cJSON_Delete(root);
}

typedef AIoT_API API;
AIoT_API API_MESS;

#endif /*INC_API_HPP_*/