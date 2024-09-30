#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pca9955.h"
#include "GARhead.h"
#include "head.h"
#include "hardware/structs/timer.h"

//#define GAR_DEBUG

GARHEAD::GARHEAD(PCA9955 *driver, SemaphoreHandle_t mutex, uint8_t gPin, uint8_t aPin, uint8_t rPin, uint8_t lPin)
{
    _driver = driver;
    _gPin = gPin;
    _aPin = aPin;
    _rPin = rPin;
    _lPin = lPin;
    _mutex = mutex;

    if(!_mutex)
    {
        _mutex = xSemaphoreCreateMutex();
    }
}

void GARHEAD::init(float gCurrent, float aCurrent, float rCurrent, float lCurrent)
{
    _color = off;

    setColorBrightness(green, 255);
    setColorBrightness(amber, 255);
    setColorBrightness(red, 255);
    setColorBrightness(lunar, 255);
    
    setHeadBrightness(255);

    //Set the max current for each LED
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _driver->setLEDcurrent(_gPin, gCurrent);
    _driver->setLEDcurrent(_aPin, aCurrent);
    _driver->setLEDcurrent(_rPin, rCurrent);
    _driver->setLEDcurrent(_lPin, lCurrent);

    _driver->setLEDbrightness(_gPin, 0);
    _driver->setLEDbrightness(_aPin, 0);
    _driver->setLEDbrightness(_rPin, 0);
    _driver->setLEDbrightness(_lPin, 0);
    xSemaphoreGive(_mutex);
}

bool GARHEAD::setHead(uint8_t color)
{
    bool rtn = true;
    uint8_t pin = 255;
    uint8_t cb = (uint8_t)(_brightness[color] * _headBrightness);

    if(color <= off && color != _color)
    {
        fade(_color, color);
        _color = color;
        rtn = !getError();
    }
    else if(color <= off)
    {
        switch (color)
        {
            case green:
                pin = _gPin;
                break;

            case amber:
                pin = _aPin;
                break;

            case red:
                pin = _rPin;
                break;

            case lunar:
                pin = _lPin;
                break;

            default:
                pin = 255;
                break;
        }

        xSemaphoreTake(_mutex, portMAX_DELAY);
        _driver->setLEDbrightness(pin, cb);
        xSemaphoreGive(_mutex);
    }

    return rtn;
}

bool GARHEAD::setHeadFromISR(uint8_t color)
{
    bool rtn = true;
    uint8_t pin = 255;
    uint8_t cb = (uint8_t)(_brightness[color] * _headBrightness);
    BaseType_t higherPriorityTaskWoken = pdFALSE;

    if(color <= off && color != _color)
    {
        fade(_color, color);
        _color = color;
        rtn = !getError();
    }
    else if(color <= off)
    {
        switch (color)
        {
            case green:
                pin = _gPin;
                break;

            case amber:
                pin = _aPin;
                break;

            case red:
                pin = _rPin;
                break;

            case lunar:
                pin = _lPin;
                break;

            default:
                pin = 255;
                break;
        }

        xSemaphoreTakeFromISR(_mutex, &higherPriorityTaskWoken);
        _driver->setLEDbrightness(pin, cb);
        xSemaphoreGiveFromISR(_mutex, &higherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(higherPriorityTaskWoken);

    return rtn;
}

uint8_t GARHEAD::getColor()
{
    return _color;
}

uint8_t GARHEAD::getError()
{
    uint8_t rtn = 0;

    xSemaphoreTake(_mutex, portMAX_DELAY);
    _driver->checkErrors();

    switch(_color)
    {
        case green:
            rtn = _driver->getError(_gPin);
            break;

        case amber:
            rtn = _driver->getError(_aPin);
            break;

        case red:
            rtn = _driver->getError(_rPin);
            break;

        case lunar:
            rtn = _driver->getError(_lPin);
            break;

        default:
            break;
    }

    xSemaphoreGive(_mutex);
    
    return rtn;
}

void GARHEAD::setColorBrightness(uint8_t color, uint8_t level)
{
    if(color != off)
    {
        _brightness[color] = level;
    }
}

void GARHEAD::setHeadBrightness(float level)
{
    _headBrightness = level/255.0;

    setHead(_color);
}

#define FADE_TIME 50
void GARHEAD::fade(uint8_t oldColor, uint8_t newColor)
{
    uint8_t ocb = (uint8_t)(_brightness[oldColor] * _headBrightness);
    uint8_t ncb = (uint8_t)(_brightness[newColor] * _headBrightness);
    uint8_t oPin, nPin, oBri, nBri = 0;

    switch (oldColor)
    {
        case green:
            oPin = _gPin;
            break;

        case amber:
            oPin = _aPin;
            break;

        case red:
            oPin = _rPin;
            break;

        case lunar:
            oPin = _lPin;
            break;

        default:
            oPin = 255;
            break;
    }

    switch (newColor)
    {
        case green:
            nPin = _gPin;
            break;

        case amber:
            nPin = _aPin;
            break;

        case red:
            nPin = _rPin;
            break;

        case lunar:
            nPin = _lPin;
            break;

        default:
            nPin = 255;
            break;
    }

    for(int i = 0; i <= FADE_TIME; i++)
    {
        oBri = (uint8_t)(ocb - ((ocb * i)/FADE_TIME));
        nBri = (uint8_t)((ncb * i)/FADE_TIME);

        xSemaphoreTake(_mutex, portMAX_DELAY);

        _driver->setLEDbrightness(oPin, oBri);
        _driver->setLEDbrightness(nPin, nBri);

        xSemaphoreGive(_mutex);

        busy_wait_ms(1);
    }
}
