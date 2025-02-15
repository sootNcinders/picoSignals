#include "ctc.h"

#include "FreeRTOS.h"
#include "task.h"

#include "hardware/watchdog.h"
#include "hardware/irq.h"

#include "main.h"
#include "battery.h"
#include "radio.h"
#include "io.h"
#include "heads.h"
#include "overlay.h"

#include <stdio.h>
#include <cmath>

bool CTC::updateNeeded = false;
bool CTC::paused = false;
bool CTC::ovlMode = false;
TaskHandle_t CTC::ctcTaskHandle;
uint8_t CTC::addr;
uint8_t CTC::tries;
uint8_t CTC::maxTries;

CTCNODEINFO CTC::ctcNodes[8];

void CTC::init(void)
{
    addr = Main::cfg["address"];
    maxTries = Main::cfg["retries"] | 10;
    tries = 0;

    ctcNodes[0].addr = addr;
    ctcNodes[0].triesRemaining = MAXTRIES;
    ctcNodes[0].responded = false;

    if(Main::mode == OVL)
    {
        ovlMode = true;
    }

    xTaskCreate(ctcTask, "CTC Task", 512, NULL, CTCPRIORITY, &ctcTaskHandle);

    DPRINTF("CTC Task Initialized\n");
}

void CTC::ctcTask(void *pvParameters)
{
    TOCTC toCTC;

    switchInfo info[8];
    switchInfo lastInfo[8];
    
    float bat;
    float lastBat;

    uint8_t caps = 0;
    uint8_t rels = 0;
    uint8_t turnouts = 0;
    uint8_t numTurnouts = 0;

    while(true)
    {
        //DPRINTF("CTC Task\n");

        IO::getSwitchInfo((switchInfo*) &info);

        caps = 0;
        rels = 0;
        turnouts = 0;
        numTurnouts = 0;

        for(uint8_t i = 0; i < MAXINPUTS; i++)
        {
            if((info[i].mode == capture || info[i].mode == turnoutCapture) && info[i].active)
            {
                caps |= (1 << info[i].headNum);
            }
            else if(info[i].mode == release && info[i].active)
            {
                rels |= (1 << info[i].headNum);
            }
            else if(info[i].mode == turnout && info[i].active)
            {
                turnouts |= (1 << numTurnouts);
                numTurnouts++;
            }
        }

        bat = Battery::getBatteryVoltage();

        if(ctcNodes[0].addr == 0)
        {
            updateNeeded = false;
        }

        if(updateNeeded && !paused)
        {
            toCTC.sender = addr;

            if(ovlMode)
            {
                toCTC.head1 = OVERLAY::getHead(0);
                toCTC.head2 = OVERLAY::getHead(1);
                toCTC.head3 = OVERLAY::getHead(2);
                toCTC.head4 = OVERLAY::getHead(3);
                toCTC.captures = IO::getOvlAuxIn();
                toCTC.releases = 0;
                toCTC.turnouts = 0;
                toCTC.avgRSSI  = 0;
                toCTC.ledError = OVERLAY::getLEDErrors();
            }
            else
            {
                toCTC.head1 = HEADS::getHead(0);
                toCTC.head2 = HEADS::getHead(1);
                toCTC.head3 = HEADS::getHead(2);
                toCTC.head4 = HEADS::getHead(3);
                toCTC.captures = caps;
                toCTC.releases = rels;
                toCTC.turnouts = turnouts;
                toCTC.avgRSSI  = Radio::getAvgRSSI();
                toCTC.ledError = HEADS::getLEDErrors();
            }

            toCTC.voltage = (uint8_t) round(bat * 10);
            lastBat = bat;
            toCTC.version = VERSION;
            toCTC.revision = REVISION;
            
            printf(":%x%x%c%c%c%c%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x\n", toCTC.sender>>4, toCTC.sender&0xF, toCTC.head1, toCTC.head2, toCTC.head3, toCTC.head4,
                    toCTC.captures, toCTC.releases, toCTC.turnouts, abs(toCTC.avgRSSI)>>4, abs(toCTC.avgRSSI)&0xF, toCTC.voltage>>4, toCTC.voltage&0xF, 
                    (toCTC.ledError>>12)&0xF, (toCTC.ledError>>8)&0xF, (toCTC.ledError>>4)&0xF, toCTC.ledError & 0xF,
                    toCTC.version>>4, toCTC.version&0xF, toCTC.revision>>4, toCTC.revision&0xF);
            Radio::sendToCTC(toCTC);

            tries++;

            if(tries > maxTries)
            {
                tries = 0;
                updateNeeded = false;

                for(uint8_t i = 0; i < MAXCTC; i++)
                {
                    if(!ctcNodes[i].responded)
                    {
                        ctcNodes[i].triesRemaining--;
                    }

                    if(ctcNodes[i].addr != 0 && ctcNodes[i].triesRemaining == 0)
                    {
                        for(uint8_t x = i; x < MAXCTC - 1; x++)
                        {
                            memcpy(&ctcNodes[x], &ctcNodes[x + 1], sizeof(CTCNODEINFO));
                        }

                        ctcNodes[MAXCTC - 1].addr = 0;
                        ctcNodes[MAXCTC - 1].triesRemaining = 0;
                        ctcNodes[MAXCTC - 1].responded = false;
                    }
                }
            }
        }
        else
        {
            if(memcmp(lastInfo, info, sizeof(info)) != 0)
            {
                updateNeeded = true;
            }

            if(bat - lastBat > 0.25 || lastBat - bat > 0.25)
            {
                updateNeeded = true;
            }
        }

        memcpy(lastInfo, info, sizeof(info));

        if(!updateNeeded)
        {
            xTaskNotifyWait(0, ULONG_MAX, NULL, 5000/portTICK_PERIOD_MS);
        }
        else
        {
            vTaskDelay(500/portTICK_PERIOD_MS);
        }
    }
}

void CTC::update(void)
{
    updateNeeded = true;
    tries = 0;

    xTaskNotifyGive(ctcTaskHandle);
}

void CTC::pause(bool on)
{
    paused = on;
}

TaskHandle_t CTC::getTaskHandle(void)
{
    return ctcTaskHandle;
}

void CTC::processFromMsg(FROMCTC msg, uint8_t from)
{
    bool allResponded = true;
    UBaseType_t priority = uxTaskPriorityGet(NULL);

    if(ctcTaskHandle)
    {
        for(uint8_t i = 0; i < MAXCTC; i++)
        {
            if(ctcNodes[i].addr == from || ctcNodes[i].addr == 0)
            {
                ctcNodes[i].addr = from;
                ctcNodes[i].triesRemaining = MAXTRIES;

                break;
            }
        }

        switch(msg.cmd)
        {
            case 0x00: //ACK
                for(uint8_t i = 0; i < MAXCTC; i++)
                {
                    if(ctcNodes[i].addr == from)
                    {
                        ctcNodes[i].responded = true;
                    }

                    if(!ctcNodes[i].responded && ctcNodes[i].addr != 0)
                    {
                        allResponded = false;
                    }
                }

                if(allResponded)
                {
                    updateNeeded = false;
                    tries = 0;
                }
                break;
                
            case 0x01: //Ping
                update();
                break;

            case 0x02: //Wake
                HEADS::wake();
                break;

            case 0x03: //Capture
            case 0x04:
            case 0x05:
            case 0x06:
                IO::setCapture(msg.cmd - 3);
                HEADS::startComm(msg.cmd - 3);
                break;

            case 0x07: //Release
            case 0x08:
            case 0x09:
            case 0x0A:
                IO::setRelease(msg.cmd - 7);
                HEADS::startComm(msg.cmd - 7);
                break;

            case 0x0B: //Clear Flash Config
                DPRINTF("Erase Config\n");

                vTaskPrioritySet(NULL, MAXPRIORITY);

                bool irq[26];

                vTaskSuspendAll();

                for(uint8_t i = 0; i < sizeof(irq); i++)
                {
                    irq[i] = irq_is_enabled(i);
                    irq_set_enabled(i, false);
                }

                Main::eraseFlashJSON();
                //flash_safe_execute((void(*)(void*))eraseFlashJSON, NULL, 10000);

                for(uint8_t i = 0; i < sizeof(irq); i++)
                {
                    irq_set_enabled(i, irq[i]);
                }

                vTaskPrioritySet(NULL, priority);

                xTaskResumeAll();
                break;
                
            case 0x0C: //Cause a watchdog reboot after 500ms to clear UART
                Main::reset();
                break;
                
        }
    }
}

void CTC::processToMsg(TOCTC msg)
{
    if(!paused)
    {
        printf(":%x%x%c%c%c%c%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x\n", msg.sender>>4, msg.sender&0xF, msg.head1, msg.head2, msg.head3, msg.head4,
                msg.captures, msg.releases, msg.turnouts, abs(msg.avgRSSI)>>4, abs(msg.avgRSSI)&0xF, msg.voltage>>4, msg.voltage&0xF, 
                (msg.ledError>>12)&0xF, (msg.ledError>>8)&0xF, (msg.ledError>>4)&0xF, msg.ledError & 0xF,
                msg.version>>4, msg.version&0xF, msg.revision>>4, msg.revision&0xF);
    }
}
