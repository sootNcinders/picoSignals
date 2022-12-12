#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pca9955.h"
#include "GARhead.h"
#include "head.h"

//#define GAR_DEBUG

GARHEAD::GARHEAD(PCA9955 *driver, uint8_t gPin, uint8_t aPin, uint8_t rPin)
{
    _driver = driver;
    _gPin = gPin;
    _aPin = aPin;
    _rPin = rPin;
}

void GARHEAD::init(float gCurrent, float aCurrent, float rCurrent)
{
    _color = off;

    setColorBrightness(green, 255);
    setColorBrightness(amber, 255);
    setColorBrightness(red, 255);
    
    setHeadBrightness(255);

    //Set the max current for each LED
    _driver->setLEDcurrent(_gPin, gCurrent);
    _driver->setLEDcurrent(_aPin, aCurrent);
    _driver->setLEDcurrent(_rPin, rCurrent);

    _driver->setLEDbrightness(_gPin, 0);
    _driver->setLEDbrightness(_aPin, 0);
    _driver->setLEDbrightness(_rPin, 0);
}

bool GARHEAD::setHead(uint8_t color)
{
    bool rtn = true;
    uint8_t b = 0;

    switch(color)
    {
        case green:
            b = (uint8_t)(_brightness[green] * _headBrightness);

            if(b < 8)
            {
                b = 8;
            }

            _driver->setLEDbrightness(_gPin, b);
            _driver->setLEDbrightness(_aPin, 0);
            _driver->setLEDbrightness(_rPin, 0);

            _driver->checkErrors();
            rtn = !_driver->getError(_gPin);

            _color = green;

            #ifdef GAR_DEBUG
            printf("GAR HEAD: Setting Green Brightness: %d Actual: %d\n", _brightness[green], b);
            #endif
            break;

        case amber:
            b = (uint8_t)(_brightness[amber] * _headBrightness);

            if(b < 8)
            {
                b = 8;
            }

            _driver->setLEDbrightness(_gPin, 0);
            _driver->setLEDbrightness(_aPin, b);
            _driver->setLEDbrightness(_rPin, 0);

            _driver->checkErrors();
            rtn = !_driver->getError(_aPin);

            _color = amber;

            #ifdef GAR_DEBUG
            printf("GAR HEAD: Setting Amber Brightness: %d Actual: %d\n", _brightness[amber], b);
            #endif
            break;

        case red:
            b = (uint8_t)(_brightness[red] * _headBrightness);

            if(b < 8)
            {
                b = 8;
            }

            _driver->setLEDbrightness(_gPin, 0);
            _driver->setLEDbrightness(_aPin, 0);
            _driver->setLEDbrightness(_rPin, b);

            _driver->checkErrors();
            rtn = !_driver->getError(_rPin);

            _color = red;

            #ifdef GAR_DEBUG
            printf("GAR HEAD: Setting Red Brightness: %d Actual: %d\n", _brightness[red], b);
            #endif
            break;
        
        default:
            _driver->setLEDbrightness(_gPin, 0);
            _driver->setLEDbrightness(_aPin, 0);
            _driver->setLEDbrightness(_rPin, 0);

            _color = off;

            #ifdef GAR_DEBUG
            printf("GAR HEAD: off\n");
            #endif
            break;
    }

    return rtn;
}

uint8_t GARHEAD::getColor()
{
    return _color;
}

uint8_t GARHEAD::getError()
{
    uint8_t rtn = 0;

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

        default:
            break;
    }
    
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
