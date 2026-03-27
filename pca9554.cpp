#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "pca9554.h"
#include "hardware/structs/timer.h"

pca9554::pca9554(i2c_inst_t *i2cbus, uint8_t addr)
{
    bus = i2cbus;
    address = addr;
}

bool pca9554::inputMask(uint8_t mask)  
{
    bool rtn = false;

    uint8_t buf[2] = {0x03, mask};

    for(int i = 0; i < 5; i++)
    {
        if(i2c_write_timeout_us(bus, address, buf, 2, false, 500) == 2)
        {
            rtn = true;
            break;
        }
        busy_wait_us(5);//Back to back access delay
    }

    busy_wait_us(5);//Back to back access delay

    return rtn;
}

bool pca9554::getInput(uint8_t num, bool update)
{
    if(update)
    {
        updateInputs();
    }

    return pinState[num];
}

void pca9554::updateInputs()
{
    int rtn;

    buffer = 0x00;

    for(int i = 0; i < 5; i++)
    {
        if(i2c_write_timeout_us(bus, address, &buffer, 1, true, 500) == 1)
        {
            rtn = true;
            break;
        }
        busy_wait_us(5);//Back to back access delay
    }
    
    for(int i = 0; i < 5; i++)
    {
        rtn = i2c_read_timeout_us(bus, address, &buffer, 1, false, 500);

        if(rtn == 1)
        {
            break;
        }

        busy_wait_us(5);//Back to back access delay
    }

    if(rtn == 1)
    {
        /*if(buffer != lastBuffer)
        {
            printf("PCA9554 read 0x%X\n", buffer);
        }
        lastBuffer = buffer;*/

        for(int i = 0; i < 8; i++)
        {
            pinState[i] = !((buffer >> i) & 0x01);
        }
    }
    else
    {
        for(int i = 0; i<8; i++)
        {
            pinState[i] = false;
        }
    }

    busy_wait_us(5);//Back to back access delay
}
