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

        static void processRemoteCLI(REMOTECLI inBuf, uint8_t from);

    private:
        static void menuTask(void *pvParameters);

        static void menuProcessor(char* inBuf, bool remote, uint8_t from);
        static void adjustmentProcessor(char* inBuf, bool remote, uint8_t from);
        static void printHelp(bool remote, uint8_t from);

        static uint8_t addr;

        static REMOTECLI remoteData[8]; //Remote CLI buffer
        static uint8_t remoteFrom[8]; //Remote CLI from address buffer
        static uint8_t remoteHead;
        static uint8_t remoteTail;
};

#endif
