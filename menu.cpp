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
#include "ArduinoJson.h"

uint8_t MENU::addr;
uint8_t MENU::remoteHead;
uint8_t MENU::remoteTail;
REMOTECLI MENU::remoteData[8]; //Remote CLI buffer
uint8_t MENU::remoteFrom[8]; //Remote CLI from address buffer

void MENU::init(void)
{
    addr = Main::cfg["address"];

    xTaskCreate(menuTask, "Menu Task", FILESIZE+4096, NULL, MENUPRIORITY, NULL);

    remoteHead = 0;
    remoteTail = 0;
    memset(remoteData, 0, sizeof(remoteData));
    memset(remoteFrom, 0, sizeof(remoteFrom));
}

void MENU::processRemoteCLI(REMOTECLI inBuf, uint8_t from)
{
    if(inBuf.isAck)
    {
        printf("~%x%x", from>>4, from&0x0F);

        for(uint8_t i = 0; i < sizeof(inBuf.data); i++)
        {
            if(inBuf.data[i] == '\n')
            {
                printf("%c", 0x1A);
            }
            
            printf("%c", inBuf.data[i]);
            
            if(inBuf.data[i] == '\n')
            {
                printf("~%x%x", from>>4, from&0x0F);
            }
        }

        printf("\n");
    }
    else
    {
        remoteData[remoteHead] = inBuf;
        remoteFrom[remoteHead] = from;
        remoteHead = (remoteHead + 1) % sizeof(remoteFrom);
    }
}

void MENU::menuTask(void *pvParameters)
{
    char cin;
    char inBuf[1024];

    uint8_t dest = 0;

    uint16_t bufIdx = 0;

    bool ctc = false;
    bool menu = false;
    bool remoteCli = false;

    FROMCTC fromCTC;

    UBaseType_t priority;

    bufIdx = 0;
    memset(inBuf, 0, sizeof(inBuf));

    while(true)
    {
        if(remoteHead != remoteTail)
        {
            priority = uxTaskPriorityGet(NULL);

            vTaskPrioritySet(NULL, MAXPRIORITY);

            if(remoteData[remoteTail].data[1] >= '0' && remoteData[remoteTail].data[1] <= '9')
            {
                adjustmentProcessor((char*)remoteData[remoteTail].data, true, remoteFrom[remoteTail]);
            }
            else
            {
                menuProcessor((char*)remoteData[remoteTail].data, true, remoteFrom[remoteTail]);
            }

            vTaskPrioritySet(NULL, priority);

            remoteTail = (remoteTail + 1) % sizeof(remoteFrom);
        }

        cin = getchar_timeout_us(0);
        while(cin >= 0x08 && cin <= 0x7F && bufIdx < sizeof(inBuf))
        {
            inBuf[bufIdx] = cin;
            
            if(bufIdx > 0)
            {
                bufIdx++;
            }

            if(cin == ':' && bufIdx == 0)
            {
                bufIdx = 1;
                ctc = true;
                menu = false;
                remoteCli = false;
            }
            else if(cin == '~' && bufIdx == 0)
            {
                bufIdx = 1;
                ctc = false;
                menu = false;
                remoteCli = true;
            }
            else if(((cin >= 'A' && cin <= 'Z') || (cin >= 'a' && cin <= 'z')) && bufIdx == 0)
            {
                bufIdx = 1;
                ctc = false;
                menu = true;
                remoteCli = false;
            }
            else if(bufIdx == 0 && (cin == '\r' || cin == '\n'))
            {
                printHelp(false, 0);
            }

            //backspace, but cant backspace over the start character
            if((cin == 0x08 || cin == 0x7F) && bufIdx > 0)
            {
                inBuf[bufIdx - 1] = 0;
                bufIdx--;
                inBuf[bufIdx - 1] = 0;
                bufIdx--;

                printf("\b");
                printf(" ");
                printf("\b");
            }
            else if(menu)
            {
                printf("%c", inBuf[bufIdx - 1]);
            }

            if(ctc && bufIdx == 5)
            {
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

                memset(inBuf, 0, sizeof(inBuf));
            }
            else if(remoteCli && bufIdx >= 2 && (cin == '\r' || cin == '\n'))
            {
                //printf("%s\n", inBuf);
                dest = 0;
                dest += ((inBuf[1] >= 'A') ? (inBuf[1] >= 'a') ? (inBuf[1] - 'a' + 10) : (inBuf[1] - 'A' + 10) : (inBuf[1] - '0')) << 4;
                dest += (inBuf[2] >= 'A') ? (inBuf[2] >= 'a') ? (inBuf[2] - 'a' + 10) : (inBuf[2] - 'A' + 10) : (inBuf[2] - '0');

                Radio::sendRemoteCLI((char*)&inBuf[3], bufIdx-3, dest, false);

                bufIdx = 0;
                memset(inBuf, 0, sizeof(inBuf));
            }
            else if(menu && bufIdx >= 2 && (cin == '\r' || cin == '\n'))
            {
                priority = uxTaskPriorityGet(NULL);

                vTaskPrioritySet(NULL, MAXPRIORITY);

                printf("\n");

                if(inBuf[1] >= '0' && inBuf[1] <= '9')
                {
                    adjustmentProcessor(inBuf, false, 0);
                }
                else
                {
                    menuProcessor(inBuf, false, 0);
                }

                vTaskPrioritySet(NULL, priority);

                bufIdx = 0;
                memset(inBuf, 0, sizeof(inBuf));
            }

            cin = getchar_timeout_us(100);
        }

        vTaskDelay(100/portTICK_PERIOD_MS);
    }
}

void MENU::menuProcessor(char* inBuf, bool remote, uint8_t from)
{
    switchInfo info[8];
    char buf[512];
    uint8_t head = 0;
    uint16_t numChars;
    UBaseType_t priority;

    memset(buf, 0, sizeof(buf));

    if(strncasecmp(inBuf, "bat", 3) == 0)
    {
        numChars = snprintf(buf, sizeof(buf), "> Battery Voltage: Current: %2.2fV 24hr AVG:%2.2fV\n", Battery::getCurrentBattery(), Battery::getBatteryVoltage());
        printf("%s", buf);

        if(remote)
        {
            Radio::sendRemoteCLI(buf, numChars, from, true);
        }
    }
    else if(strncasecmp(inBuf, "err", 3) == 0)
    {
        if(strncasecmp(inBuf+4, "clr", 3) == 0)
        {
            numChars = snprintf(buf, sizeof(buf), "> Errors Cleared\n");
            LED::setError(NOERROR);
        }
        else
        {
            numChars = snprintf(buf, sizeof(buf), "> Error Code: %d - %s\n", LED::getError(), errorCodes[LED::getError()]);
        }

        printf("%s", buf);

        if(remote)
        {
            Radio::sendRemoteCLI(buf, numChars, from, true);
        }
    }
    else if(strncasecmp(inBuf, "head", 4) == 0)
    {
        for(uint8_t i = 0; i < MAXHEADS; i++)
        {
            numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> Head %d: %c\n", i+1, HEADS::getHead(i));
        }

        printf("%s", buf);

        if(remote)
        {
            Radio::sendRemoteCLI(buf, numChars, from, true);
        }
    }
    else if(strncasecmp(inBuf, "in", 2) == 0)
    {
        IO::getSwitchInfo((switchInfo*) &info);

        for(uint8_t i = 0; i < MAXINPUTS; i++)
        {
            numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> Input %d: %s:%s %s\n", i+1, switchModes[info[i].mode], (info[i].active) ? "1" : "0", (info[i].lastActive) ? "1" : "0");
        }

        printf("%s", buf);

        if(remote)
        {
            Radio::sendRemoteCLI(buf, numChars, from, true);
        }
    }
    else if(strncasecmp(inBuf, "rssi", 4) == 0)
    {
        numChars = snprintf(buf, sizeof(buf), "> Radio RSSI: %d\n", Radio::getAvgRSSI());

        printf("%s", buf);

        if(remote)
        {
            Radio::sendRemoteCLI(buf, numChars, from, true);
        }
    }
    else if(strncasecmp(inBuf, "sys", 3) == 0)
    {
        vTaskList(buf);
        numChars = snprintf(buf, sizeof(buf), "%s\n", buf);

        printf("%s", buf);

        if(remote)
        {
            Radio::sendRemoteCLI(buf, numChars, from, true);
        }
    }
    else if(strncasecmp(inBuf, "cap", 3) == 0)
    {
        head = atoi(inBuf+4) - 1;

        if(head < MAXHEADS)
        {
            IO::setCapture(head);
            HEADS::startComm(head);
        }
    }
    else if(strncasecmp(inBuf, "rel", 3) == 0)
    {
        head = atoi(inBuf+4) - 1;

        if(head < MAXHEADS)
        {
            IO::setRelease(head);
            HEADS::startComm(head);
        }
    }
    else if(strncasecmp(inBuf, "wake", 4) == 0)
    {
        HEADS::wake();
    }
    else if(strncasecmp(inBuf, "flash", 5) == 0)
    {
        if(strncasecmp(inBuf+6, "clr", 3) == 0)
        {
            numChars = snprintf(buf, sizeof(buf), "Erase Config\n");

            printf("%s", buf);

            if(remote)
            {
                Radio::sendRemoteCLI(buf, numChars, from, true);
            }

            priority = uxTaskPriorityGet(NULL);

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
        }
    }
    else if(strncasecmp(inBuf, "rst", 3) == 0)
    {
        Main::reset();
    }
    else if(strncasecmp(inBuf, "LED", 3) == 0)
    {
        uint32_t leds = HEADS::getRawErrors();

        ledInfo* info = HEADS::getLedInfo();

        uint8_t headNum = 0;

        for(uint8_t i = 0; i < 16; i++)
        {
            headNum = info[i].headNum;

            if(i < 9)
            {
                numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "LED  %d: Head: %d ", i+1, headNum);
            }
            else
            {
                numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "LED %d: Head: %d ", i+1, headNum);
            }

            switch(info[i].color)
            {
                case notUsed:
                    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "Not Used - ");
                    break;
                
                case liGreen:
                    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "Green    - ");
                    break;

                case liAmber:
                    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "Amber    - ");
                    break;

                case liRed:
                    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "Red      - ");
                    break;

                case liBlue:
                    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "Blue     - ");
                    break;

                case liLunar:
                    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "Lunar    - ");
                    break;
            }

            if(((leds >> (i*2)) & 0x3) == LEDshort)
            {
                numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "Shorted\n");
            }
            else if(((leds >> (i*2)) & 0x3) == LEDopen)
            {
                numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "Open\n");
            }
            else
            {
                numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "OK\n");
            }
        }

        printf("%s", buf);

        if(remote)
        {
            Radio::sendRemoteCLI(buf, numChars, from, true);
        }
    }
    else if(strncasecmp(inBuf, "wrt", 3) == 0)
    {
        char cfgRaw[FILESIZE]; //May need to expand for more complex config files
        memset(cfgRaw, 0, sizeof(cfgRaw));

        serializeJsonPretty(Main::cfg, cfgRaw, FILESIZE);

        Main::writeFlashJSON((uint8_t*)cfgRaw);
        numChars = snprintf(buf, sizeof(buf), "> Config written to flash\n");

        printf("%s", buf);

        if(remote)
        {
            Radio::sendRemoteCLI(buf, numChars, from, true);
        }

        if(Main::writeSdJSON((uint8_t*)cfgRaw))
        {
            numChars = snprintf(buf, sizeof(buf), "> Config written to SD\n");
        }
        else
        {
            numChars = snprintf(buf, sizeof(buf), "> Config write to SD failed\n");
        }

        printf("%s", buf);

        if(remote)
        {
            Radio::sendRemoteCLI(buf, numChars, from, true);
        }
    }
    else if(strncasecmp(inBuf, "SET", 3) == 0)
    {
        uint8_t head = atoi(inBuf+4);

        numChars = snprintf(buf, sizeof(buf), "Set Head %d ", head); 

        if(head > 0 && head <= 4)
        {
            head--;

            if(strncasecmp(inBuf+6, "ON", 2) == 0)
            {
                HEADS::setHeadOn(head);

                numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "ON\n");
            }
            else if(strncasecmp(inBuf+6, "DIM", 3) == 0)
            {
                HEADS::setHeadDim(head);

                numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "DIM\n");
            }
            else if(strncasecmp(inBuf+6, "OFF", 3) == 0)
            {
                HEADS::setHeadOff(head);

                numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "OFF\n");
            }
            else if(strncasecmp(inBuf+6, "GREEN", 5) == 0)
            {
                HEADS::setHead(head, green);

                numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "GREEN\n");
            }
            else if(strncasecmp(inBuf+6, "AMBER", 5) == 0)
            {
                HEADS::setHead(head, amber);

                numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "AMBER\n");
            }
            else if(strncasecmp(inBuf+6, "RED", 3) == 0)
            {
                HEADS::setHead(head, red);

                numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "RED\n");
            }
            else if(strncasecmp(inBuf+6, "LUNAR", 5) == 0)
            {
                HEADS::setHead(head, lunar);

                numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "LUNAR\n");
            }
        }

        printf("%s", buf);

        if(remote)
        {
            Radio::sendRemoteCLI(buf, numChars, from, true);
        }
    }
    else if(strncasecmp(inBuf, "NODES", 5) == 0)
    {
        bool* nodes = Radio::getOnlineNodes();
        uint8_t numNodes = 0;

        numChars = snprintf(buf, sizeof(buf), "> Nodes heard in the last 90 minutes:\n");

        for(uint16_t i = 0; i < 255; i++)
        {
            if(nodes[i])
            {
                numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "%d, ", i);
                numNodes++;
                if(numNodes % 10 == 0)
                {
                    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "\n");
                }
            }
        }
        
        numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "\n");

        printf("%s", buf);

        if(remote)
        {
            Radio::sendRemoteCLI(buf, numChars, from, true);
        }
    }
    else
    {
        printHelp(remote, from);
    }
}

void MENU::adjustmentProcessor(char* inBuf, bool remote, uint8_t from)
{
    char numChar[255];
    char newChar[255];

    char buf[512];
    memset(buf, 0, sizeof(buf));
    uint16_t numChars = 0;

    uint8_t numIdx = 0;
    uint8_t newIdx = 0;

    char adj = inBuf[0];
    uint32_t adjNum = 0;
    uint32_t newVal = 0;
    float newFloat = 0;
    bool assign = false;

    for(uint8_t i = 1; inBuf[i] != 0 && i < 0xFF; i++)
    {
        if(inBuf[i] == '=')
        {
            assign = true;
        }
        else
        {
            if(assign)
            {
                newChar[newIdx++] = inBuf[i];
            }
            else
            {
                numChar[numIdx++] = inBuf[i];
            }
        }
    }

    adjNum = atoi(numChar);
    newVal = atoi(newChar);
    newFloat = atof(newChar);

    uint8_t headNum = (adjNum/100)-1;
    uint8_t subAdjNum = (adjNum%10)-2;
    char head[4][6] = {"head1", "head2", "head3", "head4"};

    uint8_t pinNum = (adjNum/10)-1;
    char pin[8][5] = {"pin1", "pin2", "pin3", "pin4", "pin5", "pin6", "pin7", "pin8"};

    switch(adj)
    {
        case 'g':
        case 'G':
            switch(adjNum)
            {
                case 1:
                    if(assign && newVal >= 1 && newVal <= 255)
                    {
                        Main::cfg["address"] = newVal;
                    }
                    
                    numChars = snprintf(buf, sizeof(buf), "G1= Address %d\n", (uint8_t)Main::cfg["address"]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;
                
                case 2:
                    if(assign && newVal >= 1 && newVal <= 255)
                    {
                        Main::cfg["retries"] = newVal;
                    }

                    if((uint8_t)Main::cfg["retries"] == 0)
                    {
                        Main::cfg["retries"] = 10;
                    }

                    numChars = snprintf(buf, sizeof(buf), "G2= Retries %d\n", (uint8_t)Main::cfg["retries"]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 3:
                    if(assign && newVal >= 1 && newVal <= 255)
                    {
                        Main::cfg["retryTime"] = newVal;
                    }

                    if((uint8_t)Main::cfg["retryTime"] == 0)
                    {
                        Main::cfg["retryTime"] = 100;
                    }

                    numChars = snprintf(buf, sizeof(buf), "G3= Retry Time %dms\n", (uint8_t)Main::cfg["retryTime"]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 4:
                    if(assign && newVal >= 1 && newVal <= 255)
                    {
                        Main::cfg["dimTime"] = newVal;
                    }

                    if((uint8_t)Main::cfg["dimTime"] == 0)
                    {
                        Main::cfg["dimTime"] = 15;
                    }

                    numChars = snprintf(buf, sizeof(buf), "G4= Dim Time %dmin\n", (uint8_t)Main::cfg["dimTime"]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 5:
                    if(assign && newVal >= 1 && newVal <= 255)
                    {
                        Main::cfg["sleepTime"] = newVal;
                    }

                    if((uint8_t)Main::cfg["sleepTime"] == 0)
                    {
                        Main::cfg["sleepTime"] = 30;
                    }

                    numChars = snprintf(buf, sizeof(buf), "G5= Sleep Time %dmin\n", (uint8_t)Main::cfg["sleepTime"]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 6: 
                    if(assign && newFloat >= 0.1 && newFloat <= 36.0)
                    {
                        Main::cfg["lowBattery"] = newFloat;
                    }

                    if((float)Main::cfg["lowBattery"] == 0)
                    {
                        Main::cfg["lowBattery"] = 11.75;
                    }

                    numChars = snprintf(buf, sizeof(buf), "G6= Low Battery %2.1fV\n", (float)Main::cfg["lowBattery"]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 7:
                    if(assign && newFloat >= 0.1 && newFloat <= 36.0)
                    {
                        Main::cfg["batteryReset"] = newFloat;
                    }

                    if((float)Main::cfg["batteryReset"] == 0)
                    {
                        Main::cfg["batteryReset"] = 12.1;
                    }

                    numChars = snprintf(buf, sizeof(buf), "G7= Low Battery Reset %2.1fV\n", (float)Main::cfg["batteryReset"]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 8:
                    if(assign && newFloat >= 0.1 && newFloat <= 36.0)
                    {
                        Main::cfg["batteryShutdown"] = newFloat;
                    }

                    if((float)Main::cfg["batteryShutdown"] == 0)
                    {
                        Main::cfg["batteryShutdown"] = 10.0;
                    }

                    numChars = snprintf(buf, sizeof(buf), "G8= Battery Shutdown %2.1fV\n", (float)Main::cfg["batteryShutdown"]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 9:
                    if(assign && newVal >= 0 && newVal <= 1)
                    {
                        Main::cfg["ctcPresent"] = newVal;
                    }

                    if((uint8_t)Main::cfg["ctcPresent"] == 0)
                    {
                        Main::cfg["ctcPresent"] = false;
                    }

                    numChars = snprintf(buf, sizeof(buf), "G9= CTC Present %d\n", (uint8_t)Main::cfg["ctcPresent"]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 10:
                    if(assign && newVal >= 0 && newVal <= 2)
                    {
                        Main::cfg["monitorLEDs"] = newVal;
                    }

                    if((uint8_t)Main::cfg["monitorLEDs"] == 0)
                    {
                        Main::cfg["monitorLEDs"] = 0;
                    }

                    numChars = snprintf(buf, sizeof(buf), "G10= LED Monitoring Mode %d\n", (uint8_t)Main::cfg["monitorLEDs"]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 11:
                    if(assign)
                    {
                        if(strncasecmp(newChar, "CTC", 3) == 0)
                        {
                            Main::cfg["mode"] = "CTC";
                        }
                        else if(strncasecmp(newChar, "OVERLAY", 7) == 0)
                        {
                            Main::cfg["mode"] = "OVERLAY";
                        }
                        else
                        {
                            Main::cfg["mode"] = "STANDARD";
                        }
                    }

                    if(Main::cfg["mode"].isNull())
                    {
                        Main::cfg["mode"] = "STANDARD";
                    }
                    
                    numChars = snprintf(buf, sizeof(buf), "G11= Mode %s\n", (const char*)Main::cfg["mode"]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 12:
                    if(assign && ((newVal >= 1 && newVal <= 16) || newVal == 255))
                    {
                        Main::cfg["awakePin"] = newVal;
                    }

                    if((uint8_t)Main::cfg["awakePin"] == 0)
                    {
                        Main::cfg["awakePin"] = 255;
                    }

                    numChars = snprintf(buf, sizeof(buf), "G12= Awake Pin %d\n", (uint8_t)Main::cfg["awakePin"]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                default:
                    numChars = snprintf(buf, sizeof(buf), "Invalid Adjustment\n");

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;
            }
            break;

        case 'h':
        case 'H':
            switch(adjNum)
            {
                case 101:
                case 201:
                case 301:
                case 401:
                    if(assign)
                    {
                        if(strncasecmp(newChar, "standard", 7) == 0)
                        {
                            Main::cfg[head[headNum]]["mode"] = "STANDARD";
                        }
                        else if(strncasecmp(newChar, "dwarf", 5) == 0)
                        {
                            Main::cfg[head[headNum]]["mode"] = "DWARF";
                        }
                        else
                        {
                            Main::cfg[head[headNum]]["mode"] = "UNUSED";
                        }
                    }

                    if(Main::cfg[head[headNum]]["mode"].isNull())
                    {
                        Main::cfg[head[headNum]]["mode"] = "UNUSED";
                    }

                    numChars = snprintf(buf, sizeof(buf), "H%d= Head %d %s\n", adjNum, headNum+1, (const char*)Main::cfg[head[headNum]]["mode"]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 102:
                case 202:
                case 302:
                case 402:
                case 103:
                case 203:  
                case 303:
                case 403:
                case 104:
                case 204:
                case 304:
                case 404:
                case 105:
                case 205:
                case 305:
                case 405:
                case 106:
                case 206:
                case 306:
                case 406:
                case 107:
                case 207:
                case 307:
                case 407:
                    if(assign && newVal >= 1 && newVal <= 255)
                    {
                        Main::cfg[head[headNum]]["destination"][subAdjNum] = newVal;
                    }

                    numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Destination[%d] %d\n", adjNum, headNum+1, subAdjNum, 
                                            (uint8_t)Main::cfg[head[headNum]]["destination"][subAdjNum]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 108:
                case 208:
                case 308:
                case 408:
                    if(assign && newVal >= 8 && newVal <= 255)
                    {
                        Main::cfg[head[headNum]]["dim"] = newVal;
                    }

                    if((uint8_t)Main::cfg[head[headNum]]["dim"] == 0)
                    {
                        Main::cfg[head[headNum]]["dim"] = 255;
                    }

                    numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Dim Brightness %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["dim"]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 109:
                case 209:
                case 309:
                case 409:
                    if(assign && newVal >= 1 && newVal <= 255)
                    {
                        Main::cfg[head[headNum]]["release"] = newVal;
                    }

                    if((uint8_t)Main::cfg[head[headNum]]["release"] == 0)
                    {
                        Main::cfg[head[headNum]]["release"] = 6;
                    }

                    numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Release Time %dmin\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["release"]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 110:
                case 210:
                case 310:
                case 410:
                    if(assign)
                    {
                        if(strncasecmp(newChar, "RGB", 3) == 0)
                        {
                            if(!Main::cfg[head[headNum]]["amber"].isNull())
                            {
                                Main::cfg[head[headNum]]["blue"] = Main::cfg[head[headNum]]["amber"];
                                Main::cfg[head[headNum]].remove("amber");
                                Main::cfg[head[headNum]]["green"].remove("brightness");
                                Main::cfg[head[headNum]]["red"].remove("brightness");
                            }
                            else
                            {
                                Main::cfg[head[headNum]]["blue"]["pin"] = 0;
                                Main::cfg[head[headNum]]["blue"]["current"] = 0;
                            }
                        }
                        else
                        {
                            if(!Main::cfg[head[headNum]]["blue"].isNull())
                            {
                                Main::cfg[head[headNum]]["amber"] = Main::cfg[head[headNum]]["blue"];
                                Main::cfg[head[headNum]].remove("blue");
                                Main::cfg[head[headNum]]["green"].remove("rgb");
                                Main::cfg[head[headNum]]["amber"].remove("rgb");
                                Main::cfg[head[headNum]]["red"].remove("rgb");
                            }
                            else
                            {
                                Main::cfg[head[headNum]]["amber"]["pin"] = 0;
                                Main::cfg[head[headNum]]["amber"]["current"] = 0;
                                Main::cfg[head[headNum]]["amber"]["brightness"] = 0;
                            }
                        }
                    }

                    if(!Main::cfg[head[headNum]]["blue"].isNull())
                    {
                        numChars = snprintf(buf, sizeof(buf), "H%d= Head %d RGB\n", adjNum, headNum+1);
                    }
                    else
                    {
                        numChars = snprintf(buf, sizeof(buf), "H%d= Head %d GAR\n", adjNum, headNum+1);
                    }

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 111:
                case 211:
                case 311:
                case 411:
                    if(assign && newVal >= 1 && newVal <= 16)
                    {
                        Main::cfg[head[headNum]]["red"]["pin"] = newVal;
                    }

                    numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Red Pin %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["red"]["pin"]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 112:
                case 212:
                case 312:
                case 412:
                    if(assign && newVal >= 0 && newVal <= 58)
                    {
                        Main::cfg[head[headNum]]["red"]["current"] = newVal;
                    }

                    numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Red Current %dmA\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["red"]["current"]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 113:
                case 213:
                case 313:
                case 413:
                    if(!Main::cfg[head[headNum]]["blue"].isNull())
                    {
                        numChars = snprintf(buf, sizeof(buf), "H%d Unavailable\n", adjNum);
                    }
                    else
                    {
                        if(assign && newVal >= 0 && newVal <= 255)
                        {
                            Main::cfg[head[headNum]]["red"]["brightness"] = newVal;
                        }
                        
                        numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Red Brightness %d\n", adjNum, headNum+1, 
                                                (uint8_t)Main::cfg[head[headNum]]["red"]["brightness"]);
                    }

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 114:
                case 214:
                case 314:
                case 414:
                    if(assign && newVal >= 1 && newVal <= 16)
                    {
                        if(!Main::cfg[head[headNum]]["blue"].isNull())
                        {
                            Main::cfg[head[headNum]]["blue"]["pin"] = newVal;
                        }
                        else
                        {
                            Main::cfg[head[headNum]]["amber"]["pin"] = newVal;
                        }
                    }

                    if(!Main::cfg[head[headNum]]["blue"].isNull())
                    {
                        numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Blue Pin %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["blue"]["pin"]);
                    }
                    else
                    {
                        numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Amber Pin %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["amber"]["pin"]);
                    }

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 115:
                case 215:
                case 315:
                case 415:
                    if(assign && newVal >= 0 && newVal <= 58)
                    {
                        if(!Main::cfg[head[headNum]]["blue"].isNull())
                        {
                            Main::cfg[head[headNum]]["blue"]["current"] = newVal;
                        }
                        else
                        {
                            Main::cfg[head[headNum]]["amber"]["current"] = newVal;
                        }
                    }

                    if(!Main::cfg[head[headNum]]["blue"].isNull())
                    {
                        numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Blue Current %dmA\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["blue"]["current"]);
                    }
                    else
                    {
                        numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Amber Current %dmA\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["amber"]["current"]);
                    }

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 116:
                case 216:
                case 316:
                case 416:
                    if(!Main::cfg[head[headNum]]["blue"].isNull())
                    {
                        numChars = snprintf(buf, sizeof(buf), "H%d Unavailable\n", adjNum);
                    }
                    else
                    {
                        if(assign && newVal >= 0 && newVal <= 255)
                        {
                            Main::cfg[head[headNum]]["amber"]["brightness"] = newVal;
                        }
                        
                        numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Amber Brightness %d\n", adjNum, headNum+1, 
                                                (uint8_t)Main::cfg[head[headNum]]["amber"]["brightness"]);
                    }

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 117:
                case 217:
                case 317:
                case 417:
                    if(assign && newVal >= 1 && newVal <= 16)
                    {
                        Main::cfg[head[headNum]]["green"]["pin"] = newVal;
                    }

                    numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Green Pin %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["green"]["pin"]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 118:
                case 218:
                case 318:
                case 418:
                    if(assign && newVal >= 0 && newVal <= 58)
                    {
                        Main::cfg[head[headNum]]["green"]["current"] = newVal;
                    }

                    numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Green Current %dmA\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["green"]["current"]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 119:
                case 219:
                case 319:
                case 419:
                    if(!Main::cfg[head[headNum]]["blue"].isNull())
                    {
                        numChars = snprintf(buf, sizeof(buf), "H%d Unavailable\n", adjNum);
                    }
                    else
                    {
                        if(assign && newVal >= 0 && newVal <= 255)
                        {
                            Main::cfg[head[headNum]]["green"]["brightness"] = newVal;
                        }
                        
                        numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Green Brightness %d\n", adjNum, headNum+1, 
                                            (uint8_t)Main::cfg[head[headNum]]["green"]["brightness"]);
                    }

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 120:
                case 220:
                case 320:
                case 420:
                    if(assign && newVal >= 0 && newVal <= 255)
                    {
                        Main::cfg[head[headNum]]["redReleaseDelay"] = newVal;
                    }

                    numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Red Release Delay %dsec\n", adjNum, headNum+1, 
                                        (uint8_t)Main::cfg[head[headNum]]["redReleaseDelay"]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 121:
                case 221:
                case 321:
                case 421:
                    if(!Main::cfg[head[headNum]]["blue"].isNull())
                    {
                        if(assign && newVal >= 0 && newVal <= 255)
                        {
                            Main::cfg[head[headNum]]["red"]["rgb"][0] = newVal;
                        }

                        numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Red R %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["red"]["rgb"][0]);
                    }
                    else
                    {
                        numChars = snprintf(buf, sizeof(buf), "H%d Unavailable\n", adjNum);
                    }

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 122:
                case 222:
                case 322:
                case 422:
                    if(!Main::cfg[head[headNum]]["blue"].isNull())
                    {
                        if(assign && newVal >= 0 && newVal <= 255)
                        {
                            Main::cfg[head[headNum]]["red"]["rgb"][1] = newVal;
                        }

                        numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Red G %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["red"]["rgb"][1]);
                    }
                    else
                    {
                        numChars = snprintf(buf, sizeof(buf), "H%d Unavailable\n", adjNum);
                    }

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 123:
                case 223:
                case 323:
                case 423:
                    if(!Main::cfg[head[headNum]]["blue"].isNull())
                    {
                        if(assign && newVal >= 0 && newVal <= 255)
                        {
                            Main::cfg[head[headNum]]["red"]["rgb"][2] = newVal;
                        }

                        numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Red B %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["red"]["rgb"][2]);
                    }
                    else
                    {
                        numChars = snprintf(buf, sizeof(buf), "H%d Unavailable\n", adjNum);
                    }

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 124:
                case 224:
                case 324:
                case 424:
                    if(!Main::cfg[head[headNum]]["blue"].isNull())
                    {
                        if(assign && newVal >= 0 && newVal <= 255)
                        {
                            Main::cfg[head[headNum]]["amber"]["rgb"][0] = newVal;
                        }

                        numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Amber R %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["amber"]["rgb"][0]);
                    }
                    else
                    {
                        numChars = snprintf(buf, sizeof(buf), "H%d Unavailable\n", adjNum);
                    }

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 125:
                case 225:
                case 325:
                case 425:
                    if(!Main::cfg[head[headNum]]["blue"].isNull())
                    {
                        if(assign && newVal >= 0 && newVal <= 255)
                        {
                            Main::cfg[head[headNum]]["amber"]["rgb"][1] = newVal;
                        }

                        numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Amber G %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["amber"]["rgb"][1]);
                    }
                    else
                    {
                        numChars = snprintf(buf, sizeof(buf), "H%d Unavailable\n", adjNum);
                    }

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 126:
                case 226:
                case 326:
                case 426:
                    if(!Main::cfg[head[headNum]]["blue"].isNull())
                    {
                        if(assign && newVal >= 0 && newVal <= 255)
                        {
                            Main::cfg[head[headNum]]["amber"]["rgb"][2] = newVal;
                        }

                        numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Amber B %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["amber"]["rgb"][2]);
                    }
                    else
                    {
                        numChars = snprintf(buf, sizeof(buf), "H%d Unavailable\n", adjNum);
                    }

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 127:
                case 227:
                case 327:
                case 427:
                    if(!Main::cfg[head[headNum]]["blue"].isNull())
                    {
                        if(assign && newVal >= 0 && newVal <= 255)
                        {
                            Main::cfg[head[headNum]]["green"]["rgb"][0] = newVal;
                        }

                        numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Green R %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["green"]["rgb"][0]);
                    }
                    else
                    {
                        numChars = snprintf(buf, sizeof(buf), "H%d Unavailable\n", adjNum);
                    }

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 128:
                case 228:
                case 328:
                case 428:
                    if(!Main::cfg[head[headNum]]["blue"].isNull())
                    {
                        if(assign && newVal >= 0 && newVal <= 255)
                        {
                            Main::cfg[head[headNum]]["green"]["rgb"][1] = newVal;
                        }

                        numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Green G %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["green"]["rgb"][1]);
                    }
                    else
                    {
                        numChars = snprintf(buf, sizeof(buf), "H%d Unavailable\n", adjNum);
                    }

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 129:
                case 229:
                case 329:
                case 429:
                    if(!Main::cfg[head[headNum]]["blue"].isNull())
                    {
                        if(assign && newVal >= 0 && newVal <= 255)
                        {
                            Main::cfg[head[headNum]]["green"]["rgb"][2] = newVal;
                        }

                        numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Green B %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["green"]["rgb"][2]);
                    }
                    else
                    {
                        numChars = snprintf(buf, sizeof(buf), "H%d Unavailable\n", adjNum);
                    }

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 130:
                case 230:
                case 330:
                case 430:
                    if(!Main::cfg[head[headNum]]["blue"].isNull())
                    {
                        if(assign && newVal >= 0 && newVal <= 255)
                        {
                            Main::cfg[head[headNum]]["lunar"]["rgb"][0] = newVal;
                        }

                        numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Lunar R %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["lunar"]["rgb"][0]);
                    }
                    else
                    {
                        numChars = snprintf(buf, sizeof(buf), "H%d Unavailable\n", adjNum);
                    }

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 131:
                case 231:
                case 331:
                case 431:
                    if(!Main::cfg[head[headNum]]["blue"].isNull())
                    {
                        if(assign && newVal >= 0 && newVal <= 255)
                        {
                            Main::cfg[head[headNum]]["lunar"]["rgb"][1] = newVal;
                        }

                        numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Lunar G %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["lunar"]["rgb"][1]);
                    }
                    else
                    {
                        numChars = snprintf(buf, sizeof(buf), "H%d Unavailable\n", adjNum);
                    }

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 132:
                case 232:
                case 332:
                case 432:
                    if(!Main::cfg[head[headNum]]["blue"].isNull())
                    {
                        if(assign && newVal >= 0 && newVal <= 255)
                        {
                            Main::cfg[head[headNum]]["lunar"]["rgb"][2] = newVal;
                        }

                        numChars = snprintf(buf, sizeof(buf), "H%d= Head %d Lunar B %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["lunar"]["rgb"][2]);
                    }
                    else
                    {
                        numChars = snprintf(buf, sizeof(buf), "H%d Unavailable\n", adjNum);
                    }

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                default:
                    numChars = snprintf(buf, sizeof(buf), "Invalid Adjustment\n");

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;
            }
            break;

        case 'i':
        case 'I':
            switch(adjNum)
            {
                case 11:
                case 21:
                case 31:
                case 41:
                case 51:
                case 61:
                case 71:
                case 81:
                    if(assign)
                    {
                        if(strncasecmp(newChar, "capture", 7) == 0)
                        {
                            Main::cfg[pin[pinNum]]["mode"] = "capture";
                        }
                        else if(strncasecmp(newChar, "release", 7) == 0)
                        {
                            Main::cfg[pin[pinNum]]["mode"] = "release";
                        }
                        else if(strncasecmp(newChar, "turnout", 7) == 0)
                        {
                            Main::cfg[pin[pinNum]]["mode"] = "turnout";
                        }
                        else if(strncasecmp(newChar, "ovlGreen", 8) == 0)
                        {
                            Main::cfg[pin[pinNum]]["mode"] = "ovlGreen";
                        }
                        else if(strncasecmp(newChar, "ovlAmber", 8) == 0)
                        {
                            Main::cfg[pin[pinNum]]["mode"] = "ovlAmber";
                        }
                        else if(strncasecmp(newChar, "ovlRed", 6) == 0)
                        {
                            Main::cfg[pin[pinNum]]["mode"] = "ovlRed";
                        }
                        else if(strncasecmp(newChar, "ovlAuxIn", 8) == 0)
                        {
                            Main::cfg[pin[pinNum]]["mode"] = "ovlAuxIn";
                        }
                        else
                        {
                            Main::cfg[pin[pinNum]]["mode"] = "unused";
                        }
                    }

                    if(Main::cfg[pin[pinNum]]["mode"].isNull())
                    {
                        Main::cfg[pin[pinNum]]["mode"] = "unused";
                    }

                    numChars = snprintf(buf, sizeof(buf), "I%d= Input %d %s\n", adjNum, adjNum/10, (const char*)Main::cfg[pin[pinNum]]["mode"]);

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;
                
                case 12:
                case 22:
                case 32:
                case 42:
                case 52:
                case 62:
                case 72:
                case 82:
                    if(strncasecmp(Main::cfg[pin[pinNum]]["mode"], "capture", 7) == 0)
                    {
                        if(assign && newVal >= 1 && newVal <= 4)
                        {
                            Main::cfg[pin[pinNum]].remove("head");
                            Main::cfg[pin[pinNum]]["head1"] = newVal;
                        }

                        numChars = snprintf(buf, sizeof(buf), "I%d= Input %d Head 1= %d\n", adjNum, adjNum/10, (uint8_t)Main::cfg[pin[pinNum]]["head1"]);
                    }
                    else if(strncasecmp(Main::cfg[pin[pinNum]]["mode"], "release", 7) == 0 ||
                                strncasecmp(Main::cfg[pin[pinNum]]["mode"], "ovlGreen", 8) == 0 ||
                                strncasecmp(Main::cfg[pin[pinNum]]["mode"], "ovlAmber", 7) == 0 ||
                                strncasecmp(Main::cfg[pin[pinNum]]["mode"], "ovlRed", 6) == 0)
                    {
                        if(assign && newVal >= 1 && newVal <= 4)
                        {
                            Main::cfg[pin[pinNum]].remove("head1");
                            Main::cfg[pin[pinNum]].remove("head2");
                            Main::cfg[pin[pinNum]]["head"] = newVal;
                        }

                        numChars = snprintf(buf, sizeof(buf), "I%d= Input %d Head= %d\n", adjNum, adjNum/10, (uint8_t)Main::cfg[pin[pinNum]]["head1"]);
                    }
                    else
                    {
                        numChars = snprintf(buf, sizeof(buf), "I%d Unavailable\n", adjNum);
                    }

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 13:
                case 23:
                case 33:
                case 43:
                case 53:
                case 63:
                case 73:
                case 83:
                    if(strncasecmp(Main::cfg[pin[pinNum]]["mode"], "capture", 7) == 0)
                    {
                        if(assign && newVal >= 1 && newVal <= 4)
                        {
                            Main::cfg[pin[pinNum]].remove("head");
                            Main::cfg[pin[pinNum]]["head2"] = newVal;
                        }

                        numChars = snprintf(buf, sizeof(buf), "I%d= Input %d Head 2= %d\n", adjNum, adjNum/10, (uint8_t)Main::cfg[pin[pinNum]]["head2"]);
                    }
                    else
                    {
                        numChars = snprintf(buf, sizeof(buf), "I%d Unavailable\n", adjNum);
                    }

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                case 14:
                case 24:
                case 34:
                case 44:
                case 54:
                case 64:
                case 74:
                case 84:
                    if(strncasecmp(Main::cfg[pin[pinNum]]["mode"], "capture", 7) == 0)
                    {
                        if(assign && newVal >= 0 && newVal <= 8)
                        {
                            Main::cfg[pin[pinNum]]["turnout"] = newVal;
                        }

                        numChars = snprintf(buf, sizeof(buf), "I%d= Input %d Turnout= %d\n", adjNum, adjNum/10, (uint8_t)Main::cfg[pin[pinNum]]["turnout"]);
                    }
                    else
                    {
                        numChars = snprintf(buf, sizeof(buf), "I%d Unavailable\n", adjNum);
                    }

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;

                default:
                    numChars = snprintf(buf, sizeof(buf), "Invalid Adjustment\n");

                    printf("%s", buf);

                    if(remote)
                    {
                        Radio::sendRemoteCLI(buf, numChars, from, true);
                    }
                    break;
            }
            break;

        default:
            numChars = snprintf(buf, sizeof(buf), "Invalid Adjustment\n");

            printf("%s", buf);

            if(remote)
            {
                Radio::sendRemoteCLI(buf, numChars, from, true);
            }
            break;
    }
}

void MENU::printHelp(bool remote, uint8_t from)
{
    UBaseType_t priority = uxTaskPriorityGet(NULL);

    char buf[1024];
    uint16_t numChars = 0;

    vTaskPrioritySet(NULL, MAXPRIORITY);

    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> Pico Signals V%dR%d: Help\n", VERSION, REVISION);
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> bat - Print Battery Voltage\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> err - Print Error Code\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> err clr - Clear Error Code\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> head - Print Head Status\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> in - Print Input Status\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> rssi - Print Radio RSSI\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> cap x - Capture head x 1-4\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> rel x - Release head x 1-4\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> wake - Wake Signal\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> flash clr - Clear Flash\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> rst - Reset\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> sys - Print Thread Status List\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> LED - Prints LED Status\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> wrt - Write config to flash and SD\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> set x y - set head x to on, dim, off, green, amber, red, or lunar\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> nodes - Print all nodes heard in the last 90min\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "\n> Adjustments\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> G1 - G12 - General Settings\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> H101 - H132 - Head 1 Settings\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> H201 - H232 - Head 2 Settings\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> H301 - H332 - Head 3 Settings\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> H401 - H432 - Head 4 Settings\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> I11 - I14 - Input 1 Settings\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> I21 - I24 - Input 2 Settings\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> I31 - I34 - Input 3 Settings\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> I41 - I44 - Input 4 Settings\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> I51 - I54 - Input 5 Settings\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> I61 - I64 - Input 6 Settings\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> I71 - I74 - Input 7 Settings\n");
    numChars += snprintf((char*)&buf[numChars], sizeof(buf) - numChars, "> I81 - I84 - Input 8 Settings\n");

    printf("%s", buf);

    if(remote)
    {
        Radio::sendRemoteCLI(buf, numChars, from, true);
    }

    vTaskPrioritySet(NULL, priority);
}
