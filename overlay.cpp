#include "overlay.h"

#include "main.h"
#include "io.h"
#include "ctc.h"
#include "radio.h"

uint8_t OVERLAY::heads[MAXHEADS];
uint8_t OVERLAY::partner;

bool OVERLAY::monOpenCircuits = false;
bool OVERLAY::monMultipleCircuits = false;

void OVERLAY::init(void) 
{
    if(Main::cfg["monitorLEDs"] == 2)
    {
        monOpenCircuits = true;
        monMultipleCircuits = true;
    }
    else if(Main::cfg["monitorLEDs"] == 1)
    {
        monOpenCircuits = true;
        monMultipleCircuits = false;
    }
    else
    {
        monOpenCircuits = false;
        monMultipleCircuits = false;
    }

    partner = Main::cfg["partner"] | 99;

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
        //Gets the current active aspect, assumes the most restrictive if multiple are active
        if(IO::getOvlR(headNum))
        {
            heads[headNum] = red;
        }
        
        else if(IO::getOvlA(headNum))
        {
            heads[headNum] = amber;
        }
        else if(IO::getOvlG(headNum))
        {
            heads[headNum] = green;
        }
        else
        {
            heads[headNum] = off;
        }

        if(heads[headNum] != lastHead)
        {
            CTC::update();

            switch (heads[headNum])
            {
            case green:
                Radio::transmit(partner, 'G', false, false);
                break;

            case amber:
                Radio::transmit(partner, 'R', false, false);
                break;

            case red:
                Radio::transmit(partner, 'A', false, false);
                break;
            
            default:
                break;
            }

            lastHead = heads[headNum];
        }

        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}

//Returns the current aspect of the head as an ASCII character
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

//Get any errors from the LEDS
uint16_t OVERLAY::getLEDErrors(void)
{
    uint16_t rtn;
    uint8_t cnt;

    //Loop through each overlay head and count the number of active overlay inputs
    for(uint8_t i = 0; i < IO::getNumOvlHeads(); i++)
    {
        cnt = 0;

        if(IO::getOvlG(i))
        {
            cnt++;
        }
        if(IO::getOvlA(i))
        {
            cnt++;
        }
        if(IO::getOvlR(i))
        {
            cnt++;
        }

        //There should only be one active LED, otherwise there is an error
        if(monOpenCircuits && cnt == 0)
        {
            rtn++;
        }

        if(monMultipleCircuits && cnt > 1)
        {
            rtn++;
        }
    }

    return rtn;
}
