#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "radio.h"

#ifndef MENU_H
#define MENU_H

class MENU
{
    public:
        static void init(void);

    private:
        static void menuTask(void *pvParameters);

        static void menuProcessor(char* inBuf);
        static void adjustmentProcessor(char* inBuf);
        static void printHelp(void);

        static uint8_t addr;
};

#endif
