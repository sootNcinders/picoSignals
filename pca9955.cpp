#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "pca9955.h"
#include "hardware/structs/timer.h"

//#define PCA9955_DEBUG

PCA9955::PCA9955(i2c_inst_t *i2cbus, uint8_t addr, float maxCurrent)
{
    _bus = i2cbus;
    _address = addr;
    _maxCurrent = maxCurrent;

    for(uint8_t i = 0; i < 16; i++)
    {
        _errors[i] = LEDnormal;
    }
}

void PCA9955::setLEDcurrent(uint8_t num, float mAmps)
{
    if(num >=0 && num <= 15)
    {
        uint8_t buffer[2];

        if(mAmps > _maxCurrent)
        {
            mAmps = _maxCurrent;
        }

        buffer[0] = 0x18 + num;
        buffer[1] = (uint8_t)((mAmps/57.375)*255);
        
        i2c_write_timeout_us(_bus, _address, buffer, 2, false, 1000);

        #ifdef PCA9955_DEBUG
        printf("PCA9955 Write Reg 0x%x 0x%x\n", buffer[0], buffer[1]);
        #endif

        busy_wait_us(10);
    }
}

void PCA9955::setLEDbrightness(uint8_t num, uint8_t brightness)
{
    if(num >= 0 && num <= 15)
    {
        uint8_t buffer[2];
        uint8_t outReg;

        #ifdef PCA9955_DEBUG
        printf("PCA9955 Set Brightness %d %d\n", num, brightness);
        #endif

        outReg = 0x02 + (uint8_t)(num/4);

        ledState[outReg-0x02]  &= ~(0x03 << ((num % 4) * 2));// LED off binary 00

        if(brightness > 247)
        {
            ledState[outReg-0x02] |= (0x01 << ((num % 4) * 2));//LED full on binary 01
        }
        else if(brightness > 8)
        {
            ledState[outReg-0x02] |= (0x02 << ((num % 4) * 2));//LED PWM on binary 10
        }

        buffer[0] = outReg;
        buffer[1] = ledState[outReg-0x02];

        #ifdef PCA9955_DEBUG
        printf("PCA9955 Write Reg 0x%x 0x%x\n", buffer[0], buffer[1]);
        #endif

        i2c_write_timeout_us(_bus, _address, buffer, 2, false, 500);

        busy_wait_us(10);

        buffer[0] = 0x08 + num;
        buffer[1] = brightness;

        #ifdef PCA9955_DEBUG
        printf("PCA9955 Write Reg 0x%x 0x%x\n", buffer[0], buffer[1]);
        #endif
        
        i2c_write_timeout_us(_bus, _address, buffer, 2, false, 500);

        busy_wait_us(10);
    }
}

bool PCA9955::checkErrors()
{
    uint8_t buffer[2];
    bool rtn = false;
    uint32_t error;
    uint8_t outReg;

    clearFaults();

    busy_wait_us(1);

    buffer[0] = 0x01;

    i2c_write_timeout_us(_bus, _address, buffer, 1, true, 500);
    i2c_read_timeout_us(_bus, _address, buffer, 1, false, 500);

    #ifdef PCA9955_DEBUG
    printf("PCA9955 Mode 2: 0x%x\n", buffer[0]);
    #endif

    busy_wait_us(10);

    if((buffer[0] >> 7) & 0x01)
    {
        #ifdef PCA9955_DEBUG
        printf("PCA9955 OVERTEMP!\n");
        #endif
        rtn = true;
    }
    if((buffer[0] >> 6) & 0x01)
    {
        error = 0;

        for(int i = 0; i < 4; i++)
        {
            buffer[0] = 0x46 + i;

            i2c_write_timeout_us(_bus, _address, buffer, 1, true, 500);
            i2c_read_timeout_us(_bus, _address, buffer, 1, false, 500);

            #ifdef PCA9955_DEBUG
            printf("PCA9955 Error %d: 0x%x\n", i, buffer[0]);
            #endif

            error |= (buffer[0] << (i*8));
            busy_wait_us(10);
        }

        uint8_t e = 0;

        for(int i = 0; i < 16; i++)
        {
            e = ((error >> (i*2)) & 0x03);

            switch(e)
            {
                case 0x01:
                    _errors[i] = LEDshort;
                    rtn = true;

                    #ifdef PCA9955_DEBUG
                    printf("PCA9955 LED %d Short Circuit\n", i);
                    #endif
                    break;

                case 0x02:
                    _errors[i] = LEDopen;
                    rtn = true;

                    #ifdef PCA9955_DEBUG
                    printf("PCA9955 LED %d Open Circuit\n", i);
                    #endif
                    break;

                default:
                    if(((ledState[(uint8_t)(i/4)] >> ((i % 4) * 2)) & 0x03) != 0x00)
                    {
                        _errors[i] = LEDnormal;
                    }
                    break;
            }
        }
    }

    return rtn;
}

bool PCA9955::checkOpenCircuits()
{
    bool rtn = false;

    if(checkErrors())
    {
        for(int i = 0; i < 16; i++)
        {
            if(_errors[i] == LEDopen)
            {
                rtn = true;
                break;
            }
        }
    }

    return rtn;
}

bool PCA9955::checkShortCircuits()
{
    bool rtn = false;

    if(checkErrors())
    {
        for(int i = 0; i < 16; i++)
        {
            if(_errors[i] == LEDshort)
            {
                rtn = true;
                break;
            }
        }
    }

    return rtn;
}

uint8_t PCA9955::getError(uint8_t num)
{
    return _errors[num];
}

void PCA9955::printRegisters()
{
    uint8_t buf[2];

    printf("PCA9955:\n");

    for(int i = 0; i <= 0x49; i++)
    {
        buf[0] = i;
        buf[1] = 0;

        i2c_write_timeout_us(_bus, _address, buf, 1, true, 500);
        i2c_read_timeout_us(_bus, _address, buf+1, 1, false, 500);

        printf("Reg 0x%x: 0x%x\n", buf[0], buf[1]);

        busy_wait_us(10);
    }
}

bool PCA9955::ping()
{
    bool rtn = true;
    uint8_t buf[2];

    buf[0] = 0x01;
    buf[1] = 0b10001001;

    if(i2c_write_timeout_us(_bus, _address, buf, 2, false, 500) == PICO_ERROR_GENERIC)
    {
        rtn = false;
    }

    return rtn;
}

void PCA9955::clearFaults()
{
    uint8_t buffer[2];
    buffer[0] = 0x01;
    buffer[1] = 0x10;

    i2c_write_timeout_us(_bus, _address, buffer, 2, false, 500);

    busy_wait_us(10);
}

void PCA9955::sleep()
{
    uint8_t i;
    uint8_t buf[2];

    #ifdef PCA9955_DEBUG
    printf("Sleeping PCA9955B\n");
    #endif

    for(i = 0; i < 16; i++)
    {
        setLEDbrightness(i, 0);
    }

    buf[0] = 0x00; //Mode 1 register;
    
    i2c_write_timeout_us(_bus, _address, buf, 1, true, 500);
    i2c_read_timeout_us(_bus, _address, buf+1, 1, false, 500);

    busy_wait_us(5);//Back to back access delay

    buf[1] |= (0x01 << 4);

    i2c_write_timeout_us(_bus, _address, buf, 2, false, 500);

    busy_wait_us(5);//Back to back access delay

    //printRegisters();
}

void PCA9955::wake()
{
    uint8_t buf[2];

    #ifdef PCA9955_DEBUG
    printf("Waking PCA9955B\n");
    #endif

    buf[0] = 0x00; //Mode 1 register;
    
    i2c_write_timeout_us(_bus, _address, buf, 1, true, 500);
    i2c_read_timeout_us(_bus, _address, buf+1, 1, false, 500);

    busy_wait_us(5);//Back to back access delay

    buf[1] &= ~(0x01 << 4);

    i2c_write_timeout_us(_bus, _address, buf, 2, false, 500);

    busy_wait_us(500); //Delay to allow oscillator to stablize

    //printRegisters();
}
