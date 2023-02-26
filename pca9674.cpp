#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "pca9674.h"
#include "utils.c"

pca9674::pca9674(i2c_inst_t *i2cbus, uint8_t addr)
{
    bus = i2cbus;
    address = addr;
}

void pca9674::inputMask(uint8_t mask)  
{
    //i2c_write_blocking(bus, address, &mask, 1, false);

    for(int i = 0; i < 5; i++)
    {
        if(critSec) critical_section_enter_blocking(critSec);
        if(i2c_write_timeout_us(bus, address, &mask, 1, false, 500) == 1)
        {
            if(critSec) critical_section_exit(critSec);
            break;
        }
        if(critSec) critical_section_exit(critSec);
        nop_sleep_us(5);//Back to back access delay
    }

    nop_sleep_us(5);//Back to back access delay
}

bool pca9674::getInput(uint8_t num, bool update)
{
    if(update)
    {
        updateInputs();
    }

    return pinState[num];
}

void pca9674::updateInputs()
{
    int rtn;
    //i2c_read_blocking(bus, address, &buffer, 1, false);

    for(int i = 0; i < 5; i++)
    {
        if(critSec) critical_section_enter_blocking(critSec);
        rtn = i2c_read_timeout_us(bus, address, &buffer, 1, false, 500);
        if(critSec) critical_section_exit(critSec);

        if(rtn == 1)
        {
            break;
        }

        nop_sleep_us(5);//Back to back access delay
    }

    if(rtn == 1)
    {
        /*if(buffer != lastBuffer)
        {
            printf("PCA9674 read 0x%X\n", buffer);
        }
        lastBuffer = buffer;*/

        for(int i = 0; i < 8; i++)
        {
            pinState[i] = !((buffer >> i) & 0x01);
        }
    }

    nop_sleep_us(5);//Back to back access delay
}

void pca9674::setCriticalSection(critical_section_t* cs)
{
    critSec = cs;
}
