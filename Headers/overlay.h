#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "main.h"

#include "head.h"

#ifndef OVERLAY_H
#define OVERLAY_H

class OVERLAY
{
    public:
        static void init(void);

        static uint8_t getHead(uint8_t headNum);

    private:
        static void overlayTask(void *pvParameters);

        static uint8_t heads[MAXHEADS];
};

#endif
