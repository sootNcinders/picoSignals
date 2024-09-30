#include "heads.h"
#include "io.h"
#include "led.h"
#include "radio.h"
#include "ctc.h"

#include "hardware/i2c.h"
#include "hardware/watchdog.h"

PCA9955 HEADS::output = PCA9955(i2c0, 0x01, 57.375);
headInfo HEADS::heads[MAXHEADS];

uint8_t HEADS::maxRetries;
uint8_t HEADS::retryTime;
uint8_t HEADS::dimTime;
uint8_t HEADS::sleepTime;
uint8_t HEADS::clearDelayTime = 10;
bool HEADS::monLEDsOpen;
bool HEADS::monLEDsShort;

bool HEADS::headsOn;
bool HEADS::headsDim;

uint8_t HEADS::awakeIndicator;

absolute_time_t HEADS::dimTimeout;

TaskHandle_t HEADS::headsTaskHandle;
TaskHandle_t HEADS::headCommTaskHandle[MAXHEADS];

void HEADS::init(void)
{
    uint8_t pin1, pin2, pin3, pin4, cur1, cur2, cur3, cur4 = 255;

    JsonObject Jhead[] = {Main::cfg["head1"].as<JsonObject>(), Main::cfg["head2"].as<JsonObject>(), Main::cfg["head3"].as<JsonObject>(), Main::cfg["head4"].as<JsonObject>()};

    //Initialize all LED pins 
    for(int i = 0; i < 16; i++)
    {
        output.setLEDcurrent(i, 0);
        output.setLEDbrightness(i, 0);
    }

    maxRetries = Main::cfg["retries"]   | 10; //Number of retries, default to 10 if not specified 
    retryTime  = Main::cfg["retryTime"] | 150; //Time to wait for a response, default to 150ms if not specified 
    dimTime    = Main::cfg["dimTime"]   | 15; //Time to dim the head(s), default to 15min if not specified
    sleepTime  = Main::cfg["sleepTime"] | 30; //Time to put the heads to sleep, 30min if not specified

    if(Main::cfg["monitorLEDs"] == 2)
    {
    monLEDsOpen = true; 
    monLEDsShort = true;
    }
    else if(Main::cfg["monitorLEDs"] == 1)
    {
    monLEDsOpen = true; 
    monLEDsShort = false;
    }   
    else
    {
        monLEDsOpen = false;
        monLEDsShort = false;
    }

    for(int i = 0; i < MAXHEADS; i++)
    {
        GARHEAD *gar; //Temporary GAR head object
        RGBHEAD *rgb; //Temporary RGB head object
        if(!Jhead[i].isNull())
        {
            //If a blue is specified, assume the head is RGB
            if(!Jhead[i]["blue"].as<JsonObject>().isNull())
            {
                uint8_t bri1, bri2, bri3 = 0;

                pin1 = Jhead[i]["red"]["pin"];
                pin1--;
                cur1 = Jhead[i]["red"]["current"];

                pin2 = Jhead[i]["green"]["pin"];
                pin2--;
                cur2 = Jhead[i]["green"]["current"];

                pin3 = Jhead[i]["blue"]["pin"];
                pin3--;
                cur3 = Jhead[i]["blue"]["current"];

                heads[i].head = new RGBHEAD(&output, IO::getIOmutex(), pin1, pin2, pin3);
                rgb = (RGBHEAD*)heads[i].head;
                rgb->init(cur1, cur2, cur3);

                rgb->setColorLevels(red, Jhead[i]["red"]["rgb"][0], Jhead[i]["red"]["rgb"][1], Jhead[i]["red"]["rgb"][2]);
                rgb->setColorLevels(amber, Jhead[i]["amber"]["rgb"][0], Jhead[i]["amber"]["rgb"][1], Jhead[i]["amber"]["rgb"][2]);
                rgb->setColorLevels(green, Jhead[i]["green"]["rgb"][0], Jhead[i]["green"]["rgb"][1], Jhead[i]["green"]["rgb"][2]);
                rgb->setColorLevels(lunar, Jhead[i]["lunar"]["rgb"][0], Jhead[i]["lunar"]["rgb"][1], Jhead[i]["lunar"]["rgb"][2]);
            }
            else
            {
                uint8_t briG, briA, briR, briL = 255;

                if(!Jhead[i]["green"].as<JsonObject>().isNull())
                {
                    pin1 = Jhead[i]["green"]["pin"];
                    pin1--;
                    cur1 = Jhead[i]["green"]["current"];
                    briG = Jhead[i]["green"]["brightness"] | 255;
                }
                else
                {
                    pin1 = 255;
                }

                if(!Jhead[i]["amber"].as<JsonObject>().isNull())
                {
                    pin2 = Jhead[i]["amber"]["pin"];
                    pin2--;
                    cur2 = Jhead[i]["amber"]["current"];
                    briA = Jhead[i]["amber"]["brightness"] | 255;
                }
                else
                {
                    pin2 = 255;
                }

                if(!Jhead[i]["red"].as<JsonObject>().isNull())
                {
                    pin3 = Jhead[i]["red"]["pin"];
                    pin3--;
                    cur3 = Jhead[i]["red"]["current"];
                    briR = Jhead[i]["red"]["brightness"] | 255;
                }
                else
                {
                    pin3 = 255;
                }

                if(!Jhead[i]["lunar"].as<JsonObject>().isNull())
                {
                    pin4 = Jhead[i]["lunar"]["pin"];
                    pin4--;
                    cur4 = Jhead[i]["lunar"]["current"];
                    briL = Jhead[i]["lunar"]["brightness"] | 255;
                }
                else
                {
                    pin4 = 255;
                }

                heads[i].head = new GARHEAD(&output, IO::getIOmutex(), pin1, pin2, pin3, pin4);
                gar = (GARHEAD*)heads[i].head;

                gar->init(cur1, cur2, cur3, cur4);
                gar->setColorBrightness(green, briG);
                gar->setColorBrightness(amber, briA);
                gar->setColorBrightness(red, briR);
                gar->setColorBrightness(lunar, briL);
            }

            //Load up to 6 destinations 
            heads[i].numDest = 0;
            for(int x = 0; x < MAXDESTINATIONS; x++)
            {
                uint8_t d = Jhead[i]["destination"][x];
                if(d != 0)
                {
                    heads[i].destAddr[heads[i].numDest] = d;
                    heads[i].numDest++;
                }
            }

            //if no valid destinations were supplied, fault out
            if(heads[i].numDest == 0)
            {
                //panic("No Destination configured\n");
                DPRINTF("No Destination configured\n");
                LED::errorLoop(CONFIGBAD);
                
            }

            heads[i].dimBright = Jhead[i]["dim"] | 255; //Set the dim brightness value, default to 255 (full brightness) if not specified 

            heads[i].releaseTime = Jhead[i]["release"] | 6; //Set the time to auto clear the head, default to 6min if not specified

            heads[i].delayClearStarted = false;
        }
    }

    if(!Main::cfg["AwakePin"].isNull())
    {
        awakeIndicator = Main::cfg["AwakePin"];
    }
    else
    {
        awakeIndicator = 255;
    }

    if(awakeIndicator < 16)
    {
        output.setLEDcurrent(awakeIndicator, 58);
        output.setLEDbrightness(awakeIndicator, 255);
    }

    for(int i = 0; i < MAXHEADS; i++)
    {
        if(heads[i].head)
        {
            for(int x = 0; x < heads[i].numDest; x++)
            {
                DPRINTF("Head %d Dest %d: %d\n", i, x, heads[i].destAddr[x]);
            }
        }
    }

    for(int i = 0; i < off; i++)
    {
        for(int x = 0; x < MAXHEADS; x++)
        {
            if(heads[x].head)
            {
                heads[x].head->setHead(i);
            }
        }
        //busy_wait_ms(1000);
        sleep_ms(1000);
    }

    //Set all heads to red to send releases so the block starts on clear.
    for(int i = 0; i < MAXHEADS; i++)
    {
        if(heads[i].head)
        {
            heads[i].head->setHead(red);
        }
    }

    if(!watchdog_caused_reboot())
    {
        for(int i = 0; i < MAXHEADS; i++)
        {
            if(heads[i].head)
            {
                IO::setRelease(i);
            }
        }
    }
    else
    {
        LED::setError(WATCHDOG);
    }

    headsOn = true;
    headsDim = false;

    xTaskCreate(headsTask, "headTask", 512, NULL, HEADSPRIORITY, &headsTaskHandle);

    DPRINTF("Heads Task Initialized\n");

    for(uint32_t i = 0; i < MAXHEADS; i++)
    {
        if(heads[i].head)
        {
            xTaskCreate(headCommTask, "headCommTask", 640, (void*)i, HEADSCOMMPRIORITY, &headCommTaskHandle[i]);

            DPRINTF("Head %d Comm Task Initialized\n", i);
        }
    }
}

void HEADS::headsTask(void *pvParameters)
{
    dimTimeout = get_absolute_time();

    while(true)
    {
        //DPRINTF("Heads Task\n");

        if(((absolute_time_diff_us(dimTimeout, get_absolute_time())/60000000) > dimTime) && headsOn && !headsDim)
        {
            for(int i = 0; i < MAXHEADS; i++)
            {
                if(heads[i].head)
                {
                    heads[i].head->setHeadBrightness(heads[i].dimBright);
                }
            }
            headsDim = true;
        }
        else if(((absolute_time_diff_us(dimTimeout, get_absolute_time())/60000000) > sleepTime) && headsOn)
        {
            for(int i = 0; i < MAXHEADS; i++)
            {
                if(heads[i].head)
                {
                    heads[i].head->setHead(off);
                }
            }
            output.sleep();
            headsOn = false;

            CTC::update();
        }

        for(int i = 0; i < MAXHEADS; i++)
        {
            if(heads[i].head)
            {
                if(heads[i].head->getColor() == red || heads[i].head->getColor() == amber)
                {
                    if((absolute_time_diff_us(heads[i].releaseTimer, get_absolute_time())/60000000) > heads[i].releaseTime)
                    {
                        //IO::setRelease(i);
                        heads[i].head->setHead(green);

                        CTC::update();
                    }
                }
            }
        }

        //Handle timer overflows
        if(absolute_time_diff_us(dimTimeout, get_absolute_time()) < 0)
        {
            dimTimeout = get_absolute_time();
        }

        for(int i = 0; i < MAXHEADS; i++)
        {
            if(absolute_time_diff_us(heads[i].releaseTimer, get_absolute_time()) < 0)
            {
                heads[i].releaseTimer = get_absolute_time();
            }
        }
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);  
    }
}

void HEADS::headCommTask(void *pvParameters)
{
    uint8_t headNum = (uint32_t)pvParameters;

    bool missingResponse = false;
    bool incompleteSend = false;
    bool transmitted = false;

    while(true)
    {
        //DPRINTF("Head %d Comm Task\n", headNum);

        missingResponse = false;
        incompleteSend = false;
        transmitted = false;

        if(heads[headNum].retries > 10)
        {
            if(heads[headNum].destResponded[0])
            {
                for(int x = 0; x < heads[headNum].numDest; x++)
                {
                    if(heads[headNum].destResponded[x] == false)
                    {
                        heads[headNum].destResponded[x] = true;
                        break;
                    }
                }

                missingResponse = false;
                for(int x = 0; x < heads[headNum].numDest; x++)
                {
                    if(heads[headNum].destResponded[x] == false)
                    {
                        missingResponse = true;
                        heads[headNum].retries = 0;
                    }
                }
                if(!missingResponse)
                {
                    heads[headNum].retries = 0;

                    IO::setLastActive(headNum, capture);
                    IO::setLastActive(headNum, turnoutCapture);
                    IO::setLastActive(headNum, release);
                    
                    for(int x = 0; x < heads[headNum].numDest; x++)
                    {
                        heads[headNum].destResponded[x] = false;
                    }
                }
            }
            else
            {
                heads[headNum].retries = 0;

                IO::setLastActive(headNum, capture);
                IO::setLastActive(headNum, turnoutCapture);
                IO::setLastActive(headNum, release);
                
                for(int x = 0; x < heads[headNum].numDest; x++)
                {
                    heads[headNum].destResponded[x] = false;
                }
            }
        }

        if(IO::getCapture(headNum))
        {
            CTC::pause(true);

            if(!headsOn || headsDim)
            {
                wake();
            }

            for(int x = 0; x < heads[headNum].numDest; x++)
            {
                if(!heads[headNum].destResponded[x] && heads[headNum].destResponded[0])
                {
                    incompleteSend = true;
                }
            }

            if(heads[headNum].head->getColor() == green || incompleteSend)
            {
                for(int x = 0; x < heads[headNum].numDest; x++)
                {
                    if(!heads[headNum].destResponded[x])
                    {
                        heads[headNum].head->setHeadBrightness(0);
                        add_alarm_in_ms(BLINKTIME, blinkOn, &heads[headNum], true);
                        Radio::transmit(heads[headNum].destAddr[x], 'R', false, false);
                        heads[headNum].retries++;
                        DPRINTF("Sending Capture %d to %d\n", headNum, heads[headNum].destAddr[x]);
                        transmitted = true;
                        break;
                    }
                }

                IO::setLastActive(headNum, release);
            }
        }
        
        if(IO::getRelease(headNum))
        {
            CTC::pause(true);

            for(int x = 0; x < heads[headNum].numDest; x++)
            {
                if(!heads[headNum].destResponded[x] && heads[headNum].destResponded[0])
                {
                    incompleteSend = true;
                }
            }

            if((heads[headNum].head->getColor() != green && heads[headNum].head->getColor() != off) || incompleteSend)
            {
                for(int x = 0; x < heads[headNum].numDest; x++)
                {
                    if(!heads[headNum].destResponded[x])
                    {
                        heads[headNum].head->setHeadBrightness(0);
                        add_alarm_in_ms(BLINKTIME, blinkOn, &heads[headNum], true);
                        Radio::transmit(heads[headNum].destAddr[x], 'G', false, false);
                        heads[headNum].retries++;
                        DPRINTF("Sending Release %d to %d\n", headNum, heads[headNum].destAddr[x]);
                        transmitted = true;
                        break;
                    }
                }

                IO::setLastActive(headNum, capture);
                IO::setLastActive(headNum, turnoutCapture);
            }
        }

        if(IO::getCapture(headNum) || IO::getRelease(headNum))
        {
            vTaskDelay(retryTime/portTICK_PERIOD_MS);
        }
        else
        {
            CTC::pause(false);

            xTaskNotifyWait(0, ULONG_MAX, NULL, portMAX_DELAY);
        }
    }
}

void HEADS::processRxMsg(RCL msg, uint8_t from)
{
    bool headFound = false;
    bool localRelease = false;
    uint8_t headNum = 0;
    uint8_t addr = Radio::getAddr();

    for(int i = 0; i < MAXHEADS && !headFound; i++)
    {
        for(int x = 0; x < heads[i].numDest && !headFound; x++)
        {
            if(from == heads[i].destAddr[x])
            {
                headNum = i;
                headFound = true;
            }
        }
    }

    //Process if this node was the destination and the transmitting node is a destination for one of the heads
    if(msg.destination == addr && !msg.isCode && headFound && from != addr)
    {
        DPRINTF("Head %d received %c from %d\n", headNum, msg.aspect, from);
        switch(msg.aspect)
        {
            case 'G':
            case 'g':
                //Respond if necessary 
                if(!msg.isACK)
                {
                    Radio::transmit(from, 'G', true, false);
                }
                
                localRelease = IO::getRelease(headNum);

                setLastActive(headNum, from, release);

                if(heads[headNum].head->getColor() == amber && !localRelease)
                {
                    if(!heads[headNum].delayClearStarted)
                    {
                        add_alarm_in_ms((clearDelayTime * 1000), delayedClear, &heads[headNum], true);
                        heads[headNum].delayClearStarted = true;
                    }
                }
                else
                {
                    heads[headNum].head->setHead(green);
                    CTC::update();
                }

                if(!headsOn)
                {
                    output.wake();

                    for(int i = 0; i < MAXHEADS; i++)
                    {
                        if(heads[i].head)
                        {
                            heads[i].head->setHeadBrightness(255);

                            if(heads[i].head->getColor() == off)
                            {
                                heads[i].head->setHead(green);
                            }
                        }
                    }

                    if(awakeIndicator < 16)
                    {
                        output.setLEDbrightness(awakeIndicator, 255);
                    }

                    headsOn = true;
                    headsDim = false;
                }
                else if(headsDim)
                {
                    for(int i = 0; i < MAXHEADS; i++)
                    {
                        if(heads[i].head)
                        {
                            heads[i].head->setHeadBrightness(255);
                        }
                    }

                    headsDim = false;
                }

                heads[headNum].retries = 0;
                dimTimeout = get_absolute_time();
                break;

            case 'A':
            case 'a':
                if(!msg.isACK)
                {
                    Radio::transmit(from, 'R', true, false);
                }

                if(heads[headNum].head->getColor() == green || heads[headNum].head->getColor() == amber || heads[headNum].head->getColor() == off)
                {                                   
                    heads[headNum].head->setHead(amber);

                    CTC::update();

                    setLastActive(headNum, from, capture);

                    heads[headNum].retries = 0;
                    dimTimeout = get_absolute_time();
                    heads[headNum].releaseTimer = get_absolute_time();
                }
                break;
            
            case 'R':
            case 'r':
                bool lostRace = false;
                if(from > addr)
                {
                    if(IO::getCapture(headNum))
                    {
                        lostRace = true;
                    }
                }
                
                if((heads[headNum].head->getColor() != amber || heads[headNum].delayClearStarted) && !lostRace)
                {
                    if(!msg.isACK)
                    {
                        Radio::transmit(from, 'A', true, false);
                    }
                    
                    heads[headNum].head->setHead(red);

                    CTC::update();

                    IO::setLastActive(headNum, release);
                    IO::setLastActive(headNum, capture);
                    IO::setLastActive(headNum, turnoutCapture);

                    heads[headNum].retries = 0;
                    dimTimeout = get_absolute_time();

                    heads[headNum].releaseTimer = get_absolute_time();
                }
                else
                {
                    Radio::transmit(from, 'R', true, false);
                }
                break;
        }
    }

}

//Function to handle when the transmission sequence is complete for multiple destinations
void HEADS::setLastActive(uint8_t hN, uint8_t f, uint8_t m)
{
    //Set that the destination f has responded
    for(int i = 0; i < heads[hN].numDest; i++)
    {
        if(heads[hN].destAddr[i] == f)
        {
            heads[hN].destResponded[i] = true;
        }
    }

    //Check if any of the destinations have not responded
    bool missingResponse = false;
    for(int i = 0; i < heads[hN].numDest; i++)
    {
        if(heads[hN].destResponded[i] == false)
        {
            missingResponse = true;
            heads[i].retries = 0;
        }
    }
    //If all of the destinations have responded, set lastActive to prevent further transmissions
    if(!missingResponse)
    {
        if(m == capture)
        {
            IO::setLastActive(hN, capture);
            IO::setLastActive(hN, turnoutCapture);
        }
        else
        {
            IO::setLastActive(hN, m);
        }

        IO::setLastActive(hN, m);
        for(int x = 0; x < heads[hN].numDest; x++)
        {
            heads[hN].destResponded[x] = false;
        }
    }
}

void HEADS::wake(void)
{
    if(!headsOn || headsDim)
    {
        for(int i = 0; i < MAXHEADS; i++)
        {
            if(heads[i].head)
            {
                heads[i].head->setHeadBrightness(255);

                if(!headsOn)
                {
                    heads[i].head->setHead(green);
                }
            }
        }

        headsOn = true;
        headsDim = false;

        dimTimeout = get_absolute_time();
    }
}

void HEADS::dim(void)
{
    if(headsOn)
    {
        for(int i = 0; i < MAXHEADS; i++)
        {
            if(heads[i].head)
            {
                heads[i].head->setHeadBrightness(heads[i].dimBright);
            }
        }
    }
}

void HEADS::sleep(void)
{
    for(int i = 0; i < MAXHEADS; i++)
    {
        if(heads[i].head)
        {
            heads[i].head->setHead(off);
        }
    }

    headsOn = false;
}

int64_t HEADS::blinkOn(alarm_id_t id, void *user_data)
{
    if(user_data)
    {
        ((headInfo*)user_data)->head->setHeadBrightness(255);
    }

    return 0;
}

void HEADS::startComm(uint8_t headNum)
{
    if(heads[headNum].head)
    {
        for(int x = 0; x < MAXDESTINATIONS; x++)
        {
            heads[headNum].destResponded[x] = false;
        }
        heads[headNum].retries = 0;

        xTaskNotifyGive(headCommTaskHandle[headNum]);
    }
}

char HEADS::getHead(uint8_t headNum)
{
    char rtn = 'O';
    if(heads[headNum].head)
    {
        switch(heads[headNum].head->getColor())
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
    }
    
    return rtn;
}

uint16_t HEADS::getLEDErrors()
{
    uint16_t rtn = 0;

    xSemaphoreTake(IO::getIOmutex(), portMAX_DELAY);

    output.checkErrors();
    if(monLEDsOpen && output.checkOpenCircuits())
    {
        for(uint8_t i = 0; i < 16; i++)
        {
            if(output.getError(i) != LEDopen)
            {
                rtn |= (1 << i);
            }
        }
    }
    if(monLEDsShort && output.checkShortCircuits())
    {
        for(uint8_t i = 0; i < 16; i++)
        {
            if(output.getError(i) != LEDshort)
            {
                rtn |= (1 << i);
            }
        }
    }

    xSemaphoreGive(IO::getIOmutex());

    return rtn;
}

//alarm call back function to set the head to green from amber after the delay
int64_t HEADS::delayedClear(alarm_id_t id, void *user_data)
{
    if(user_data)
    {
        if(((headInfo*)user_data)->head->getColor() != red) //stop the head from clearing if the block was captured before the timer finished
        {
            DPRINTF("Delayed clear\n");
            ((headInfo*)user_data)->head->setHeadFromISR(green);
            CTC::update();
        }
        ((headInfo*)user_data)->delayClearStarted = false;
    }

    return 0;
}
