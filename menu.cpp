#include "menu.h"

#include "ctc.h"
#include "heads.h"
#include "io.h"
#include "radio.h"
#include "battery.h"
#include "led.h"

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

    uint8_t bufIdx = 0;

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
                DPRINTF("%c%c%c%c%c\n", inBuf[0], inBuf[1], inBuf[2], inBuf[3], inBuf[4]);
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
            else if(menu && bufIdx >= 2)
            {
                priority = uxTaskPriorityGet(NULL);

                vTaskPrioritySet(NULL, MAXPRIORITY);

                printf("\n");
                switch(inBuf[1])
                {
                    case 'b':
                    case 'B':
                        printf("> Battery Voltage: %2.2fV\n", Battery::getBatteryVoltage());
                        break;
                    case 'c':
                    case 'C':
                        printf("> Errors Cleared\n");
                        LED::setError(NOERROR);
                        break;
                    case 'd':
                    case 'D':
                        dprint = !dprint;
                        printf("> Debug: %s\n", dprint ? "On" : "Off");
                        break;
                    case 'e':
                    case 'E':
                        printf("> Error Code: %d - %s\n", LED::getError(), errorCodes[LED::getError()]);
                        break;
                    case 'h':
                    case 'H':
                        for(uint8_t i = 0; i < MAXHEADS; i++)
                        {
                            printf("> Head %d: %c\n", i, HEADS::getHead(i));
                        }
                        break;
                    case 'i':
                    case 'I':
                        IO::getSwitchInfo((switchInfo*) &info);

                        for(uint8_t i = 0; i < MAXINPUTS; i++)
                        {
                            printf("> Input %d: %s:%s\n", i, switchModes[info[i].mode], (info[i].active) ? "1" : "0");
                        }
                        break;
                    case 'r':
                    case 'R':
                        printf("> Radio RSSI: %d\n", Radio::getAvgRSSI());
                        break;
                    case 's':
                    case 'S':
                        char buf[40*MAXPRIORITY];
                        vTaskList(buf);
                        printf("%s\n", buf);
                        break;
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

    printf("> Pico Signals V%dR%d:\n", VERSION, REVISION);
    printf("> > Command Start Character\n");
    printf("> >b - Print Battery Voltage\n");
    printf("> >c - Clear Error\n");
    //printf("> >d - Toggle Debug Mode\n");
    printf("> >e - Print Error Code\n");
    printf("> >h - Print Head Status\n");
    printf("> >i - Print Input Status\n");
    printf("> >r - Print Radio RSSI\n");

    vTaskPrioritySet(NULL, priority);
}
