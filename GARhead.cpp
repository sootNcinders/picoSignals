#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pca9955.h"
#include "GARhead.h"
#include "head.h"
#include "hardware/structs/timer.h"

//#define GAR_DEBUG

GARHEAD::GARHEAD(PCA9955 *driver, uint8_t gPin, uint8_t aPin, uint8_t rPin, uint8_t lPin)
{
    _driver = driver;
    _gPin = gPin;
    _aPin = aPin;
    _rPin = rPin;
    _lPin = lPin;
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
    _driver->setLEDcurrent(_gPin, gCurrent);
    _driver->setLEDcurrent(_aPin, aCurrent);
    _driver->setLEDcurrent(_rPin, rCurrent);
    _driver->setLEDcurrent(_lPin, lCurrent);

    _driver->setLEDbrightness(_gPin, 0);
    _driver->setLEDbrightness(_aPin, 0);
    _driver->setLEDbrightness(_rPin, 0);
    _driver->setLEDbrightness(_lPin, 0);
}

bool GARHEAD::setHead(uint8_t color)
{
    bool rtn = true;
    /*uint8_t b = 0;

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
            _driver->setLEDbrightness(_lPin, 0);

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
            _driver->setLEDbrightness(_lPin, 0);

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
            _driver->setLEDbrightness(_lPin, 0);

            _driver->checkErrors();
            rtn = !_driver->getError(_rPin);

            _color = red;

            #ifdef GAR_DEBUG
            printf("GAR HEAD: Setting Red Brightness: %d Actual: %d\n", _brightness[red], b);
            #endif
            break;

        case lunar:
            b = (uint8_t)(_brightness[lunar] * _headBrightness);

            if(b < 8)
            {
                b = 8;
            }

            _driver->setLEDbrightness(_gPin, 0);
            _driver->setLEDbrightness(_aPin, 0);
            _driver->setLEDbrightness(_rPin, 0);
            _driver->setLEDbrightness(_lPin, b);

            _driver->checkErrors();
            rtn = !_driver->getError(_lPin);

            _color = red;

            #ifdef GAR_DEBUG
            printf("GAR HEAD: Setting Red Brightness: %d Actual: %d\n", _brightness[red], b);
            #endif
            break;
        
        default:
            _driver->setLEDbrightness(_gPin, 0);
            _driver->setLEDbrightness(_aPin, 0);
            _driver->setLEDbrightness(_rPin, 0);
            _driver->setLEDbrightness(_lPin, 0);


            _color = off;

            #ifdef GAR_DEBUG
            printf("GAR HEAD: off\n");
            #endif
            break;
    }*/

    if(color <= off && color != _color)
    {
        fade(_color, color);
        _color = color;
        rtn = !getError();
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

        _driver->setLEDbrightness(oPin, oBri);
        _driver->setLEDbrightness(nPin, nBri);

        busy_wait_ms(1);
    }
}
