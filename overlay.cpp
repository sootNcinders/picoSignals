#include "overlay.h"

#include "main.h"
#include "io.h"
#include "ctc.h"

uint8_t OVERLAY::heads[MAXHEADS];

void OVERLAY::init(void) 
{
    for(uint8_t i = 0; i < MAXHEADS; i++)
    {
        heads[i] = off;
    }

    for(uint32_t i = 0; i < IO::getNumOvlHeads(); i++)
    {
        xTaskCreate(overlayTask, "overlayTask", 400, (void*)i, (HEADSCOMMPRIORITY + MAXHEADS) - i, NULL);

        DPRINTF("Overlay Head %d Task Initialized\n", i+1);
    }
}

void OVERLAY::overlayTask(void *pvParameters)
{
    uint8_t headNum = (uint32_t)pvParameters;

    uint8_t lastHead;

    while(true)
    {
        if(IO::getOvlG(headNum))
        {
            heads[headNum] = green;
        }
        else if(IO::getOvlA(headNum))
        {
            heads[headNum] = amber;
        }
        else if(IO::getOvlR(headNum))
        {
            heads[headNum] = red;
        }
        else
        {
            heads[headNum] = off;
        }

        if(heads[headNum] != lastHead)
        {
            CTC::update();

            lastHead = heads[headNum];
        }

        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}

uint8_t OVERLAY::getHead(uint8_t headNum)
{
    char rtn = 'O';

    switch(heads[headNum])
    {
        case green:
            rtn = 'G';
            break;
        case amber:
            rtn = 'A';
            break;
        case red:
            rtn = 'R';
            break;
        case lunar:
            rtn = 'L';
            break;
        default:
            rtn = 'O';
            break;
    }
    
    return rtn;
}