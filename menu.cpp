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

void MENU::init(void)
{
    addr = Main::cfg["address"];

    xTaskCreate(menuTask, "Menu Task", FILESIZE+4096, NULL, MENUPRIORITY, NULL);
}

void MENU::menuTask(void *pvParameters)
{
    char cin;
    char inBuf[255];

    uint8_t bufIdx = 0;

    bool ctc = false;
    bool menu = false;

    FROMCTC fromCTC;

    UBaseType_t priority;

    while(true)
    {
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
            }
            else if(((cin >= 'A' && cin <= 'Z') || (cin >= 'a' && cin <= 'z')) && bufIdx == 0)
            {
                bufIdx = 1;
                ctc = false;
                menu = true;
            }
            else if(bufIdx == 0 && (cin == '\r' || cin == '\n'))
            {
                printHelp();
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
            else if(menu && bufIdx >= 2 && (cin == '\r' || cin == '\n'))
            {
                priority = uxTaskPriorityGet(NULL);

                vTaskPrioritySet(NULL, MAXPRIORITY);

                printf("\n");

                if(inBuf[1] >= '0' && inBuf[1] <= '9')
                {
                    adjustmentProcessor(inBuf);
                }
                else
                {
                    menuProcessor(inBuf);
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

void MENU::menuProcessor(char* inBuf)
{
    switchInfo info[8];
    char buf[40*MAXPRIORITY];
    uint8_t head = 0;
    UBaseType_t priority;

    if(strncasecmp(inBuf, "bat", 3) == 0)
    {
        printf("> Battery Voltage: %2.2fV\n", Battery::getBatteryVoltage());
    }
    else if(strncasecmp(inBuf, "err", 3) == 0)
    {
        if(strncasecmp(inBuf+4, "clr", 3) == 0)
        {
            printf("> Errors Cleared\n");
            LED::setError(NOERROR);
        }
        else
        {
            printf("> Error Code: %d - %s\n", LED::getError(), errorCodes[LED::getError()]);
        }
    }
    else if(strncasecmp(inBuf, "head", 4) == 0)
    {
        for(uint8_t i = 0; i < MAXHEADS; i++)
        {
            printf("> Head %d: %c\n", i+1, HEADS::getHead(i));
        }
    }
    else if(strncasecmp(inBuf, "in", 2) == 0)
    {
        IO::getSwitchInfo((switchInfo*) &info);

        for(uint8_t i = 0; i < MAXINPUTS; i++)
        {
            printf("> Input %d: %s:%s %s\n", i, switchModes[info[i].mode], (info[i].active) ? "1" : "0", (info[i].lastActive) ? "1" : "0");
        }
    }
    else if(strncasecmp(inBuf, "rssi", 4) == 0)
    {
        printf("> Radio RSSI: %d\n", Radio::getAvgRSSI());
    }
    else if(strncasecmp(inBuf, "sys", 3) == 0)
    {
        vTaskList(buf);
        printf("%s\n", buf);
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
            DPRINTF("Erase Config\n");

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
                printf("LED  %d: Head: %d ", i+1, headNum);
            }
            else
            {
                printf("LED %d: Head: %d ", i+1, headNum);
            }

            switch(info[i].color)
            {
                case notUsed:
                    printf("Not Used - ");
                    break;
                
                case liGreen:
                    printf("Green    - ");
                    break;

                case liAmber:
                    printf("Amber    - ");
                    break;

                case liRed:
                    printf("Red      - ");
                    break;

                case liBlue:
                    printf("Blue     - ");
                    break;

                case liLunar:
                    printf("Lunar    - ");
                    break;
            }

            if(((leds >> (i*2)) & 0x3) == LEDshort)
            {
                printf("Shorted\n");
            }
            else if(((leds >> (i*2)) & 0x3) == LEDopen)
            {
                printf("Open\n");
            }
            else
            {
                printf("OK\n");
            }
        }
    }
    else if(strncasecmp(inBuf, "wrt", 3) == 0)
    {
        char cfgRaw[FILESIZE]; //May need to expand for more complex config files
        memset(cfgRaw, 0, sizeof(cfgRaw));

        serializeJsonPretty(Main::cfg, cfgRaw, FILESIZE);

        Main::writeFlashJSON((uint8_t*)cfgRaw);
        printf("> Config written to flash\n");

        if(Main::writeSdJSON((uint8_t*)cfgRaw))
        {
            printf("> Config written to SD\n");
        }
        else
        {
            printf("> Config write to SD failed\n");
        }
    }
    else if(strncasecmp(inBuf, "SET", 3) == 0)
    {
        uint8_t head = atoi(inBuf+4);

        printf("Set Head %d ", head); 

        if(head > 0 && head < 4)
        {
            head--;

            if(strncasecmp(inBuf+6, "ON", 2) == 0)
            {
                HEADS::setHeadOn(head);

                printf("ON\n");
            }
            else if(strncasecmp(inBuf+6, "DIM", 3) == 0)
            {
                HEADS::setHeadDim(head);

                printf("DIM\n");
            }
            else if(strncasecmp(inBuf+6, "OFF", 3) == 0)
            {
                HEADS::setHeadOff(head);

                printf("OFF\n");
            }
            else if(strncasecmp(inBuf+6, "GREEN", 5) == 0)
            {
                HEADS::setHead(head, green);

                printf("GREEN\n");
            }
            else if(strncasecmp(inBuf+6, "AMBER", 5) == 0)
            {
                HEADS::setHead(head, amber);

                printf("AMBER\n");
            }
            else if(strncasecmp(inBuf+6, "RED", 3) == 0)
            {
                HEADS::setHead(head, red);

                printf("RED\n");
            }
            else if(strncasecmp(inBuf+6, "LUNAR", 5) == 0)
            {
                HEADS::setHead(head, lunar);

                printf("LUNAR\n");
            }
        }
    }
    else
    {
        printHelp();
    }
}

void MENU::adjustmentProcessor(char* inBuf)
{
    char numChar[255];
    char newChar[255];

    uint8_t numIdx = 0;
    uint8_t newIdx = 0;

    char adj = inBuf[0];
    uint32_t adjNum = 0;
    uint32_t newVal = 0;
    float newFloat = 0;
    bool assign = false;

    for(uint8_t i = 1; inBuf[i] != 0; i++)
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
                    
                    printf("G1= Address %d\n", (uint8_t)Main::cfg["address"]);
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

                    printf("G2= Retries %d\n", (uint8_t)Main::cfg["retries"]);
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

                    printf("G3= Retry Time %dms\n", (uint8_t)Main::cfg["retryTime"]);
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

                    printf("G4= Dim Time %dmin\n", (uint8_t)Main::cfg["dimTime"]);
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

                    printf("G5= Sleep Time %dmin\n", (uint8_t)Main::cfg["sleepTime"]);
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

                    printf("G6= Low Battery %2.1fV\n", (float)Main::cfg["lowBattery"]);
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

                    printf("G7= Low Battery Reset %2.1fV\n", (float)Main::cfg["batteryReset"]);
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

                    printf("G8= Battery Shutdown %2.1fV\n", (float)Main::cfg["batteryShutdown"]);
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

                    printf("G9= CTC Present %d\n", (uint8_t)Main::cfg["ctcPresent"]);
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

                    printf("G10= LED Monitoring Mode %d\n", (uint8_t)Main::cfg["monitorLEDs"]);
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
                    
                    printf("G11= Mode %s\n", (const char*)Main::cfg["mode"]);
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

                    printf("G12= Awake Pin %d\n", (uint8_t)Main::cfg["awakePin"]);
                    break;

                default:
                    printf("Invalid Adjustment\n");
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
                        else
                        {
                            Main::cfg[head[headNum]]["mode"] = "UNUSED";
                        }
                    }

                    if(Main::cfg[head[headNum]]["mode"].isNull())
                    {
                        Main::cfg[head[headNum]]["mode"] = "UNUSED";
                    }

                    printf("H%d= Head %d %s\n", adjNum, headNum+1, (const char*)Main::cfg[head[headNum]]["mode"]);
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

                    printf("H%d= Head %d Destination[%d] %d\n", adjNum, headNum+1, subAdjNum, (uint8_t)Main::cfg[head[headNum]]["destination"][subAdjNum]);
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

                    printf("H%d= Head %d Dim Brightness %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["dim"]);
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

                    printf("H%d= Head %d Release Time %dmin\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["release"]);
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
                            }
                            else
                            {
                                Main::cfg[head[headNum]]["blue"]["pin"] = 0;
                                Main::cfg[head[headNum]]["blue"]["current"] = 0;
                                Main::cfg[head[headNum]]["blue"]["brightness"] = 0;
                            }
                        }
                        else
                        {
                            if(!Main::cfg[head[headNum]]["blue"].isNull())
                            {
                                Main::cfg[head[headNum]]["amber"] = Main::cfg[head[headNum]]["blue"];
                                Main::cfg[head[headNum]].remove("blue");
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
                        printf("H%d= Head %d RGB\n", adjNum, headNum+1);
                    }
                    else
                    {
                        printf("H%d= Head %d GAR\n", adjNum, headNum+1);
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

                    printf("H%d= Head %d Red Pin %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["red"]["pin"]);
                    break;

                case 112:
                case 212:
                case 312:
                case 412:
                    if(assign && newVal >= 0 && newVal >= 58)
                    {
                        Main::cfg[head[headNum]]["red"]["current"] = newVal;
                    }

                    printf("H%d= Head %d Red Current %dmA\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["red"]["current"]);
                    break;

                case 113:
                case 213:
                case 313:
                case 413:
                    if(assign && newVal >= 0 && newVal <= 255)
                    {
                        Main::cfg[head[headNum]]["red"]["brightness"] = newVal;
                    }

                    printf("H%d= Head %d Red Brightness %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["red"]["brightness"]);
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
                        printf("H%d= Head %d Blue Pin %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["blue"]["pin"]);
                    }
                    else
                    {
                        printf("H%d= Head %d Amber Pin %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["amber"]["pin"]);
                    }
                    break;

                case 115:
                case 215:
                case 315:
                case 415:
                    if(assign && newVal >= 0 && newVal >= 58)
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
                        printf("H%d= Head %d Blue Current %dmA\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["blue"]["current"]);
                    }
                    else
                    {
                        printf("H%d= Head %d Amber Current %dmA\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["amber"]["current"]);
                    }
                    break;

                case 116:
                case 216:
                case 316:
                case 416:
                    if(assign && newVal >= 0 && newVal <= 255)
                    {
                        if(!Main::cfg[head[headNum]]["blue"].isNull())
                        {
                            Main::cfg[head[headNum]]["blue"]["brightness"] = newVal;
                        }
                        else
                        {
                            Main::cfg[head[headNum]]["amber"]["brightness"] = newVal;
                        }
                    }

                    if(!Main::cfg[head[headNum]]["blue"].isNull())
                    {
                        printf("H%d= Head %d Blue Brightness %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["blue"]["brightness"]);
                    }
                    else
                    {
                        printf("H%d= Head %d Amber Brightness %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["amber"]["brightness"]);
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

                    printf("H%d= Head %d Green Pin %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["green"]["pin"]);
                    break;

                case 118:
                case 218:
                case 318:
                case 418:
                    if(assign && newVal >= 0 && newVal >= 58)
                    {
                        Main::cfg[head[headNum]]["green"]["current"] = newVal;
                    }

                    printf("H%d= Head %d Green Current %dmA\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["green"]["current"]);
                    break;

                case 119:
                case 219:
                case 319:
                case 419:
                    if(assign && newVal >= 0 && newVal <= 255)
                    {
                        Main::cfg[head[headNum]]["green"]["brightness"] = newVal;
                    }

                    printf("H%d= Head %d Green Brightness %d\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["green"]["brightness"]);
                    break;

                case 120:
                case 220:
                case 320:
                case 420:
                    if(assign && newVal >= 0 && newVal <= 255)
                    {
                        Main::cfg[head[headNum]]["redReleaseDelay"] = newVal;
                    }

                    printf("H%d= Head %d Red Release Delay %dsec\n", adjNum, headNum+1, (uint8_t)Main::cfg[head[headNum]]["redReleaseDelay"]);
                    break;

                default:
                    printf("Invalid Adjustment\n");
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

                    printf("I%d= Input %d %s\n", adjNum, adjNum/10, (const char*)Main::cfg[pin[pinNum]]["mode"]);
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

                        printf("I%d= Input %d Head 1= %d\n", adjNum, adjNum/10, (uint8_t)Main::cfg[pin[pinNum]]["head1"]);
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

                        printf("I%d= Input %d Head= %d\n", adjNum, adjNum/10, (uint8_t)Main::cfg[pin[pinNum]]["head1"]);
                    }
                    else
                    {
                        printf("I%d Unavailable\n", adjNum);
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

                        printf("I%d= Input %d Head 2= %d\n", adjNum, adjNum/10, (uint8_t)Main::cfg[pin[pinNum]]["head2"]);
                    }
                    else
                    {
                        printf("I%d Unavailable\n", adjNum);
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

                        printf("I%d= Input %d Turnout= %d\n", adjNum, adjNum/10, (uint8_t)Main::cfg[pin[pinNum]]["turnout"]);
                    }
                    else
                    {
                        printf("I%d Unavailable\n", adjNum);
                    }
                    break;

                default:
                    printf("Invalid Adjustment\n");
            }
            break;

        default:
            printf("Invalid Adjustment\n");
            break;
    }
}

void MENU::printHelp(void)
{
    UBaseType_t priority = uxTaskPriorityGet(NULL);

    vTaskPrioritySet(NULL, MAXPRIORITY);

    printf("> Pico Signals V%dR%d: Help\n", VERSION, REVISION);
    //printf("> > Command Start Character\n");
    printf("> bat - Print Battery Voltage\n");
    printf("> err - Print Error Code\n");
    printf("> err clr - Clear Error Code\n");
    printf("> head - Print Head Status\n");
    printf("> in - Print Input Status\n");
    printf("> rssi - Print Radio RSSI\n");
    printf("> cap x - Capture head x 1-4\n");
    printf("> rel x - Release head x 1-4\n");
    printf("> wake - Wake Signal\n");
    printf("> flash clr - Clear Flash\n");
    printf("> rst - Reset\n");
    printf("> sys - Print Thread Status List\n");
    printf("> LED - Prints LED Status\n");
    printf("> wrt - Write config to flash and SD\n");
    printf("> set x y - set head x to on, dim, off, green, amber, red, or lunar\n");
    printf("> Adjustments\n");
    printf("> G1 - G12 - General Settings\n");
    printf("> H101 - H120 - Head 1 Settings\n");
    printf("> H201 - H220 - Head 2 Settings\n");
    printf("> H301 - H320 - Head 3 Settings\n");
    printf("> H401 - H420 - Head 4 Settings\n");
    printf("> I11 - I14 - Input 1 Settings\n");
    printf("> I21 - I24 - Input 2 Settings\n");
    printf("> I31 - I34 - Input 3 Settings\n");
    printf("> I41 - I44 - Input 4 Settings\n");
    printf("> I51 - I54 - Input 5 Settings\n");
    printf("> I61 - I64 - Input 6 Settings\n");
    printf("> I71 - I74 - Input 7 Settings\n");
    printf("> I81 - I84 - Input 8 Settings\n");

    vTaskPrioritySet(NULL, priority);
}
