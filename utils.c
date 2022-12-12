#include "pico/stdlib.h"

static void nop_sleep_us(uint64_t us)
{
    for(uint64_t i = 0; i < us; i++)
    {
        for(uint8_t x = 0; x < 25; x++)
        {
            asm volatile("nop");
        }
    }
}

static void nop_sleep_ms(uint16_t ms)
{
    for(uint16_t i = 0; i < ms; i++)
    {
        nop_sleep_us(1000);
    }
}