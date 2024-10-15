#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pca9955.h"
#include "RGBhead.h"

//#define RGB_DEBUG

RGBHEAD::RGBHEAD(PCA9955 *driver, SemaphoreHandle_t mutex, uint8_t rPin, uint8_t gPin, uint8_t bPin)
{
    _driver = driver;

    _rPin = rPin;
    _gPin = gPin;
    _bPin = bPin;

    _mutex = mutex;

    if(!_mutex)
    {
        _mutex = xSemaphoreCreateMutex();
    }
}

void RGBHEAD::init(float rCurrent, float gCurrent, float bCurrent)
{
    _color = off;

    _levels[green][r] = 0;
    _levels[green][g] = 255;
    _levels[green][b] = 0;

    _levels[amber][r] = 170;
    _levels[amber][g] = 85;
    _levels[amber][b] = 0;

    _levels[red][r] = 255;
    _levels[red][g] = 0;
    _levels[red][b] = 0;

    _levels[lunar][r] = 100;
    _levels[lunar][g] = 75;
    _levels[lunar][b] = 80;

    setHeadBrightness(255);

    //Set the max current for each LED
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _driver->setLEDcurrent(_rPin, rCurrent);
    _driver->setLEDcurrent(_gPin, gCurrent);
    _driver->setLEDcurrent(_bPin, bCurrent);

    _driver->setLEDbrightness(_rPin, 0);
    _driver->setLEDbrightness(_gPin, 0);
    _driver->setLEDbrightness(_bPin, 0);
    xSemaphoreGive(_mutex);
}

bool RGBHEAD::setHead(uint8_t color)
{
    bool rtn = true;
    uint8_t brightness = 0;

    switch(color)
    {
        case green:
        case amber:
        case red:
        case lunar:

            #ifdef RGB_DEBUG
            printf("RGB Setting color %d ", color);
            #endif

            brightness = (uint8_t)(_levels[color][r] * _headBrightness);

            if(brightness < 8 && brightness > 0)
            {
                brightness = 8;
            }

            xSemaphoreTake(_mutex, portMAX_DELAY);
            _driver->setLEDbrightness(_rPin, brightness);
            xSemaphoreGive(_mutex);

            #ifdef RGB_DEBUG
            printf("R: Set: %d Actual: %d ", _levels[color][r],brightness);
            #endif

            brightness = (uint8_t)(_levels[color][g] * _headBrightness);

            if(brightness < 8 && brightness > 0)
            {
                brightness = 8;
            }

            xSemaphoreTake(_mutex, portMAX_DELAY);
            _driver->setLEDbrightness(_gPin, brightness);
            xSemaphoreGive(_mutex);

            #ifdef RGB_DEBUG
            printf("G: Set: %d Actual: %d ", _levels[color][g],brightness);
            #endif

            brightness = (uint8_t)(_levels[color][b] * _headBrightness);
            
            if(brightness < 8 && brightness > 0)
            {
                brightness = 8;
            }

            xSemaphoreTake(_mutex, portMAX_DELAY);
            _driver->setLEDbrightness(_bPin, brightness);
            xSemaphoreGive(_mutex);

            #ifdef RGB_DEBUG
            printf("B: Set: %d Actual: %d ", _levels[color][b],brightness);
            #endif

            xSemaphoreTake(_mutex, portMAX_DELAY);
            _driver->checkErrors();

            if(_driver->getError(_rPin) || _driver->getError(_gPin) || _driver->getError(_bPin))
            {
                rtn = false;
            }
            xSemaphoreGive(_mutex);

            _color = color;
            break;
        
        default:

            xSemaphoreTake(_mutex, portMAX_DELAY);
            _driver->setLEDbrightness(_rPin, 0);
            _driver->setLEDbrightness(_gPin, 0);
            _driver->setLEDbrightness(_bPin, 0);
            xSemaphoreGive(_mutex);

            _color = off;
            break;
    }

    return rtn;
}

bool RGBHEAD::setHeadFromISR(uint8_t color)
{
    bool rtn = true;
    uint8_t brightness = 0;
    BaseType_t higherPriorityTaskWoken = pdFALSE;

    switch(color)
    {
        case green:
        case amber:
        case red:
        case lunar:

            #ifdef RGB_DEBUG
            printf("RGB Setting color %d ", color);
            #endif

            brightness = (uint8_t)(_levels[color][r] * _headBrightness);

            if(brightness < 8 && brightness > 0)
            {
                brightness = 8;
            }

            xSemaphoreTakeFromISR(_mutex, &higherPriorityTaskWoken);
            _driver->setLEDbrightness(_rPin, brightness);
            //xSemaphoreGiveFromISR(_mutex, &higherPriorityTaskWoken);

            #ifdef RGB_DEBUG
            printf("R: Set: %d Actual: %d ", _levels[color][r],brightness);
            #endif

            brightness = (uint8_t)(_levels[color][g] * _headBrightness);

            if(brightness < 8 && brightness > 0)
            {
                brightness = 8;
            }

            //xSemaphoreTakeFromISR(_mutex, &higherPriorityTaskWoken);            
            _driver->setLEDbrightness(_gPin, brightness);
            //xSemaphoreGiveFromISR(_mutex, &higherPriorityTaskWoken);

            #ifdef RGB_DEBUG
            printf("G: Set: %d Actual: %d ", _levels[color][g],brightness);
            #endif

            brightness = (uint8_t)(_levels[color][b] * _headBrightness);
            
            if(brightness < 8 && brightness > 0)
            {
                brightness = 8;
            }

            //xSemaphoreTakeFromISR(_mutex, &higherPriorityTaskWoken);
            _driver->setLEDbrightness(_bPin, brightness);
            //xSemaphoreGiveFromISR(_mutex, &higherPriorityTaskWoken);

            #ifdef RGB_DEBUG
            printf("B: Set: %d Actual: %d ", _levels[color][b],brightness);
            #endif

            //xSemaphoreTakeFromISR(_mutex, &higherPriorityTaskWoken);
            _driver->checkErrors();

            if(_driver->getError(_rPin) || _driver->getError(_gPin) || _driver->getError(_bPin))
            {
                rtn = false;
            }
            //xSemaphoreGiveFromISR(_mutex, &higherPriorityTaskWoken);
            xSemaphoreGive(_mutex);

            _color = color;
            break;
        
        default:

            xSemaphoreTakeFromISR(_mutex, &higherPriorityTaskWoken);
            _driver->setLEDbrightness(_rPin, 0);
            _driver->setLEDbrightness(_gPin, 0);
            _driver->setLEDbrightness(_bPin, 0);
            //xSemaphoreGiveFromISR(_mutex, &higherPriorityTaskWoken);

            xSemaphoreGive(_mutex);

            _color = off;
            break;
    }
    
    portYIELD_FROM_ISR(higherPriorityTaskWoken);

    return rtn;
}

uint8_t RGBHEAD::getColor()
{
    return _color;
}

uint8_t RGBHEAD::getError()
{
    uint8_t rtn = 0;

    xSemaphoreTake(_mutex, portMAX_DELAY);

    rtn |= (_driver->getError(_rPin) & 0x03) << 4;
    rtn |= (_driver->getError(_gPin) & 0x03) << 2;
    rtn |= (_driver->getError(_bPin) & 0x03);

    xSemaphoreGive(_mutex);

    return rtn;
}

void RGBHEAD::setColorLevels(uint8_t color, uint8_t rLevel, uint8_t gLevel, uint8_t bLevel)
{
    if(color != off)
    {
        _levels[color][r] = rLevel;
        _levels[color][g] = gLevel;
        _levels[color][b] = bLevel;
    }
}

void RGBHEAD::setHeadBrightness(float brightness)
{
    _headBrightness = brightness/255.0;

    setHead(_color);
}

void RGBHEAD::setHeadBrightnessFromISR(float brightness)
{
    _headBrightness = brightness/255.0;

    setHeadFromISR(_color);
}
