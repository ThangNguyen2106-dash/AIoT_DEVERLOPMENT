#include <IoT/Handler.hpp>
#include <cstring>
#include <Arduino.h>

// ===== Default fallback =====
void WidgetWrite_Default(uint8_t pin, const Param &param)
{
    // Bạn có thể log ở đây
    // printf("No handler for V%d\n", pin);
}

// ======================================================
// ===== TỰ SINH WEAK HANDLER =====
// ======================================================
#define X(n)                                                                   \
    void __attribute__((weak)) WidgetWrite##n(uint8_t pin, const Param &param) \
    {                                                                          \
        WidgetWrite_Default(pin, param);                                       \
    }

VPIN_LIST
#undef X

// ======================================================
// ===== TẠO VECTOR HANDLER =====
// ======================================================
#define X(n) WidgetWrite##n,

const handlerWidget_pin HandlerVector[] = {
    VPIN_LIST};

#undef X

// ===== COUNT =====
const uint8_t HandlerCount =
    sizeof(HandlerVector) / sizeof(HandlerVector[0]);

//==========================================================//
HandlerEntry DataHandler::handlers[50];
uint8_t DataHandler::count = 0;

void DataHandler::add(
    const char *key,
    handlerWidget_key cb)
{
    if (count >= 50)
        return;

    handlers[count].key = key;
    handlers[count].handler = cb;
    count++;
}

void DataHandler::dispatch(
    const char *key,
    const Param &param)
{
    for (uint8_t i = 0; i < count; i++)
    {
        if (strcmp(
                handlers[i].key,
                key) == 0)
        {
            handlers[i].handler(param);
            return;
        }
    }
}
