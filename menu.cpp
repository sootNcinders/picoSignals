#include "menu.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ctc.h"
#include "heads.h"
#include "io.h"
#include "radio.h"
#include "battery.h"
#include "led.h"
#include "main.h"

uint8_t MENU::addr;

void MENU::init(void)
{
    addr = Main::cfg["address"];

    xTaskCreate(menuTask, "Menu Task", 512, NULL, MENUPRIORITY, NULL);
}

void MENU::menuTask(void *pvParameters)
{
    char cin;
    char inBuf[255];
    char buf[40*MAXPRIORITY];

    uint8_t bufIdx = 0;
    uint8_t head = 0;

    bool ctc = false;
    bool menu = false;

    FROMCTC fromCTC;

    switchInfo info[8];

    UBaseType_t priority;

    while(true)
    {
        cin = getchar_timeout_us(0);
        while(cin >= 0x0A && cin <= 0x7F && bufIdx < sizeof(inBuf))
        {
            inBuf[bufIdx] = cin;
            
            if(bufIdx > 0)
            {
                bufIdx++;
            }

            if(cin == ':')
            {
                bufIdx = 1;
                ctc = true;
                menu = false;
            }
            else if(cin == '>')
            {
                bufIdx = 1;
                ctc = false;
                menu = true;
            }
            else if(bufIdx == 0)
            {
                printHelp();
            }

            if(menu)
            {
                printf("%c", inBuf[bufIdx - 1]);
            }

            if(ctc && bufIdx == 5)
            {
                //DPRINTF("%c%c%c%c%c\n", inBuf[0], inBuf[1], inBuf[2], inBuf[3], inBuf[4]);
                fromCTC.dest = 0;
                fromCTC.dest += ((inBuf[1] >= 'A') ? (inBuf[1] >= 'a') ? (inBuf[1] - 'a' + 10) : (inBuf[1] - 'A' + 10) : (inBuf[1] - '0')) << 4;
                fromCTC.dest += (inBuf[2] >= 'A') ? (inBuf[2] >= 'a') ? (inBuf[2] - 'a' + 10) : (inBuf[2] - 'A' + 10) : (inBuf[2] - '0');

                fromCTC.cmd = 0;
                fromCTC.cmd += ((inBuf[3] >= 'A') ? (inBuf[3] >= 'a') ? (inBuf[3] - 'a' + 10) : (inBuf[3] - 'A' + 10) : (inBuf[3] - '0')) << 4;
                fromCTC.cmd += (inBuf[4] >= 'A') ? (inBuf[4] >= 'a') ? (inBuf[4] - 'a' + 10) : (inBuf[4] - 'A' + 10) : (inBuf[4] - '0');

                DPRINTF("Dest: %d CMD: %d\n", fromCTC.dest, fromCTC.cmd);

                bufIdx = 0;

                if(fromCTC.dest == addr || fromCTC.dest == RFM95_BROADCAST_ADDR)
                {
                    CTC::processFromMsg(fromCTC, addr);

                    if(fromCTC.dest == RFM95_BROADCAST_ADDR)
                    {
                        Radio::sendFromCTC(fromCTC);
                    }
                }
                else
                {
                    Radio::sendFromCTC(fromCTC);
                }
            }
            else if(menu && bufIdx >= 2 && (cin == '\r' || cin == '\n'))
            {
                priority = uxTaskPriorityGet(NULL);

                vTaskPrioritySet(NULL, MAXPRIORITY);

                printf("\n");

                if(strncasecmp(inBuf+1, "bat", 3) == 0)
                {
                    printf("> Battery Voltage: %2.2fV\n", Battery::getBatteryVoltage());
                }
                else if(strncasecmp(inBuf+1, "err", 3) == 0)
                {
                    if(strncasecmp(inBuf+5, "clr", 3) == 0)
                    {
                        printf("> Errors Cleared\n");
                        LED::setError(NOERROR);
                    }
                    else
                    {
                        printf("> Error Code: %d - %s\n", LED::getError(), errorCodes[LED::getError()]);
                    }
                }
                else if(strncasecmp(inBuf+1, "head", 4) == 0)
                {
                    for(uint8_t i = 0; i < MAXHEADS; i++)
                    {
                        printf("> Head %d: %c\n", i+1, HEADS::getHead(i));
                    }
                }
                else if(strncasecmp(inBuf+1, "in", 2) == 0)
                {
                    IO::getSwitchInfo((switchInfo*) &info);

                    for(uint8_t i = 0; i < MAXINPUTS; i++)
                    {
                        printf("> Input %d: %s:%s\n", i, switchModes[info[i].mode], (info[i].active) ? "1" : "0");
                    }
                }
                else if(strncasecmp(inBuf+1, "rssi", 4) == 0)
                {
                    printf("> Radio RSSI: %d\n", Radio::getAvgRSSI());
                }
                else if(strncasecmp(inBuf+1, "sys", 3) == 0)
                {
                    vTaskList(buf);
                    printf("%s\n", buf);
                }
                else if(strncasecmp(inBuf+1, "cap", 3) == 0)
                {
                    head = atoi(inBuf+4) - 1;

                    if(head < MAXHEADS)
                    {
                        IO::setCapture(head);
                        HEADS::startComm(head);
                    }
                }
                else if(strncasecmp(inBuf+1, "rel", 3) == 0)
                {
                    head = atoi(inBuf+4) - 1;

                    if(head < MAXHEADS)
                    {
                        IO::setRelease(head);
                        HEADS::startComm(head);
                    }
                }
                else if(strncasecmp(inBuf+1, "wake", 4) == 0)
                {
                    HEADS::wake();
                }
                else if(strncasecmp(inBuf+1, "flash", 5) == 0)
                {
                    if(strncasecmp(inBuf+6, "clr", 3) == 0)
                    {
                        Main::eraseFlashJSON();
                        printf("> Flash Cleared\n");
                    }
                }
                else if(strncasecmp(inBuf+1, "rst", 3) == 0)
                {
                    Main::reset();
                }
                else
                {
                    printHelp();
                }

                vTaskPrioritySet(NULL, priority);

                bufIdx = 0;
            }

            cin = getchar_timeout_us(100);
        }

        vTaskDelay(100/portTICK_PERIOD_MS);
    }
}

void MENU::printHelp(void)
{
    UBaseType_t priority = uxTaskPriorityGet(NULL);

    vTaskPrioritySet(NULL, MAXPRIORITY);

    printf("> Pico Signals V%dR%d: Help\n", VERSION, REVISION);
    printf("> > Command Start Character\n");
    printf("> >bat - Print Battery Voltage\n");
    printf("> >err - Print Error Code\n");
    printf("> >err clr - Clear Error Code\n");
    printf("> >head - Print Head Status\n");
    printf("> >in - Print Input Status\n");
    printf("> >rssi - Print Radio RSSI\n");
    printf("> >cap x - Capture head x 1-4\n");
    printf("> >rel x - Release head x 1-4\n");
    printf("> >wake - Wake Signal\n");
    printf("> >flash clr - Clear Flash\n");
    printf("> >rst - Reset\n");

    vTaskPrioritySet(NULL, priority);
}
