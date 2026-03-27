#include "input.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"

inputChip Input::getInputChip(i2c_inst_t *i2cbus, uint8_t addr)
{
    uint32_t id = 0;
    inputChip chip = unknownchip;
    bool rtn = false;
    uint8_t bytes = 0;
    uint8_t devAddr = addr << 1;

    for(int i = 0; i < 5; i++)
    {
        bytes = 0;
        bytes += i2c_write_timeout_us(i2cbus, 0x7C, &devAddr, 1, true, 500);
        bytes += i2c_read_timeout_us(i2cbus, 0x7C, (uint8_t*)&id, 3, false, 500);

        if(bytes == 4)
        {
            printf("ID: 0x%08X\n", id);
            rtn = true;
            break;
        }

        busy_wait_us(5);//Back to back access delay
    }

    busy_wait_us(5);//Back to back access delay

    switch(id)
    {
        case PCA9674_ID:
            chip = pca9674chip;
            break;
        case PCA9554_ID:
        default:
            chip = pca9554chip;
            break;
    }

    return chip;
}