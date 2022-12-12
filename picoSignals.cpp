#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/adc.h"
#include "inits.h"
#include "pca9674.h"
#include "pca9955.h"
#include "RFM95/RFM95.h"
#include "head.h"
#include "GARhead.h"
#include "RGBhead.h"
#include "f_util.h"
#include "ff.h"
#include "sd_card.h"
#include "hw_config.h"
#include "ArduinoJson.h"


#define MAXHEADS 4
#define MAXINPUTS 8

//Pin State defines
#define HIGH 1
#define LOW 0

//Pin Definitions 
#define RADIOINT 15
#define SWITCHINT 3

#define GOODLED  6
#define ERRORLED 7
#define RXLED    8
#define TXLED    9
#define ADCIN    26

//Battery voltage conversion factor
#define CONVERSION_FACTOR (12.7/1374) //((3.3f / 0xFFF) * 10)

//input type
typedef enum
{
    unused = 0, 
    capture, //Capture input
    turnoutCapture, //Capture input, target based on turnout state
    release, //Release input
    turnout //Turnout input
}switchMode;

//input pin info
typedef struct
{
    uint8_t mode; //From switchMode
    uint8_t headNum; //Head number, 0 to MAXHEADS
    uint8_t headNum2; //Second head for turnout controlled captures
    uint8_t turnoutPinNum; //Pin to check state for turnout controlled captures
    bool    active; //Current pin state
    bool    lastActive; //Previous pin state
    bool    serviced = false; 
}switchInfo;

//signal head info
typedef struct
{
    Head    *head; //The head driver
    uint8_t destAddr; //this heads partner
    uint8_t dimBright; //Dimmed brightness level
    uint8_t releaseTime; //Time to wait to release
    absolute_time_t releaseTimer; //Absolute time of capture
    bool delayClearStarted;
}headInfo;


//Packet Structure
typedef struct
{
  uint8_t destination; //Unsigned 8 bit integer destination address, 0-255
  //uint8_t voltage; //Unsinged 8 bit integer battery voltage, 0-255 representing 00.0 - 25.5V
  bool isACK;  //Is this packet an acknowledgement
  bool isCode; //Is this packet a code control packet
  char aspect; /*New aspect color for signal head. R - red, A - amber, G - green
                *New aspect for right head on signal bridge: Q - red, B - amber, H - green
                *If isCode is true
                *A - Amber LED out
                *C - Remote capture
                *D - Remote release
                *G - Green LED out
                *R - Red LED out
                *W - Wake signal
                *Z - Trigger a reset - not working?
                *S - Switch Main
                *s - Switch Diverging
                *T - Switch 2 Main
                *t - Switch 2 Diverging 
                *V - Battery good reset - **needs implementing**
                *v - Battery low alarm - **needs implementing**
                */
} RCL;
RCL transmission; // The packet

//Message data
uint8_t to;
uint8_t from;
uint8_t len;

//This nodes address 
uint8_t addr = 0;

//Input pin states
switchInfo inputs[MAXINPUTS];

//IC drivers
pca9674 input1(i2c0, 0x20);
PCA9955 output1(i2c0, 0x01, 57.375);
RFM95 radio(spi0, PICO_DEFAULT_SPI_CSN_PIN, RADIOINT, 0);

headInfo heads[MAXHEADS];

bool headOn = true;
bool headDim = false;

uint8_t retries = 0;
uint8_t maxRetries = 10; 
uint8_t retryTime = 100;//in milliseconds
uint8_t dimTime = 10; //in minutes
uint8_t sleepTime = 15;//in minutes
uint8_t clearDelayTime = 10;//in seconds

int64_t tDiff = 0;

absolute_time_t retryTimeout;
absolute_time_t dimTimeout;

volatile bool alarmSet = false;

int64_t turnOnStat(alarm_id_t id, void *user_data)
{
    gpio_put(PICO_DEFAULT_LED_PIN, HIGH);
    alarmSet = false;

    return 0;
}

int64_t delayedClear(alarm_id_t id, void *user_data)
{
    if(user_data)
    {
        if(((headInfo*)user_data)->head->getColor() != red)
        {
            ((headInfo*)user_data)->head->setHead(green);
            ((headInfo*)user_data)->delayClearStarted = false;
        }
    }

    return 0;
}

void gpio_isr(uint gpio, uint32_t event_mask)
{
    //printf("ISR GPIO: %d MASK: %d\n", gpio, event_mask);

    switch(gpio)
    {
        case RADIOINT:
            //if(event_mask == RFM95_INT_MODE)
            {
                radio.handleInterrupt();
            }
            break;

        case SWITCHINT:
            //if(event_mask == PCA9674_INT_MODE)
            /*{
                if(!alarmSet)
                {
                    gpio_put(PICO_DEFAULT_LED_PIN, LOW);
                    add_alarm_in_ms(50, turnOnStat, NULL, true);
                    alarmSet = true;
                }
                
                input1.updateInputs();

                for(int i = 0; i < MAXINPUTS; i++)
                {
                    //Latch capture or release signals
                    if(inputs[i].mode == capture)
                    {
                        if(input1.getInput(i, false) == LOW && (heads[inputs[i].headNum].head->getColor() == green || heads[inputs[i].headNum].head->getColor() == off))
                        {
                            inputs[i].active = true;
                            inputs[i].lastActive = false;
                        }
                    }
                    else if(inputs[i].mode == release)
                    {
                        if(input1.getInput(i, false) == LOW && (heads[inputs[i].headNum].head->getColor() == amber || heads[inputs[i].headNum].head->getColor() == red))
                        {
                            inputs[i].active = true;
                            inputs[i].lastActive = false;
                        }
                    }
                    //get direct input for turnout monitoring
                    else if(inputs[i].mode == turnout)
                    {
                        inputs[i].active = input1.getInput(i, false);
                    }
                }

                input1.inputMask(0xFF);
            }*/
            break;
    }

    gpio_acknowledge_irq(gpio, event_mask);
}

void transmit(uint8_t dest, char asp, bool ack, bool code)
{
    //printf("TRANSMIT\n");

    transmission.destination = dest;
    transmission.aspect = asp;
    transmission.isACK = ack;
    transmission.isCode = code;

    if(!radio.send(255, (uint8_t*) &transmission, sizeof(transmission)))
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
    }
}

void parseConfig()
{
    FIL file; 
    FRESULT fr;
    FATFS fs;
    sd_card_t *pSD;
    GARHEAD *gar;
    RGBHEAD *rgb;
    
    pSD = sd_get_by_num(0);

    sd_init_driver();

    fr = f_mount(&fs, "0:", 1);
    if (fr != FR_OK)
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    const char* const filename = "config.json";
    fr = f_open(&file, filename, FA_READ);
    if (fr != FR_OK && fr != FR_EXIST)
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    }

    char cfgRaw[512];

    fr = f_read(&file, &cfgRaw, sizeof(cfgRaw), 0);

    if(fr != FR_OK)
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        panic("f_read(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    }

    StaticJsonDocument<512> cfg;
    DeserializationError error = deserializeJson(cfg, cfgRaw);
    if(error)
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        panic("Config JSON error: %s\n", error.c_str());
    }

    addr = cfg["address"];
    heads[0].destAddr = cfg["destination"];

    maxRetries = cfg["retries"] | 10;

    retryTime = cfg["retryTime"] | 100;

    dimTime = cfg["dimTime"] | 15;

    sleepTime = cfg["sleepTime"] | 30;

    if(addr == 0)
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        panic("Invalid Address\n");
    }

    uint8_t pin1, pin2, pin3, cur1, cur2, cur3 = 0;

    JsonObject Jhead = cfg["head0"].as<JsonObject>();

    if(!Jhead.isNull())
    {
        if(!Jhead["blue"].as<JsonObject>().isNull())
        {
            uint8_t bri1, bri2, bri3 = 0;

            pin1 = Jhead["red"]["pin"];
            cur1 = Jhead["red"]["current"];

            pin2 = Jhead["green"]["pin"];
            cur2 = Jhead["green"]["current"];

            pin3 = Jhead["blue"]["pin"];
            cur3 = Jhead["blue"]["current"];

            rgb = new RGBHEAD(&output1, pin1, pin2, pin3);
            rgb->init(cur1, cur2, cur3);

            //TO DO color levels

            heads[0].head = rgb;
        }
        else
        {
            uint8_t briG, briA, briR = 0;

            pin1 = Jhead["green"]["pin"];
            cur1 = Jhead["green"]["current"];
            briG = Jhead["green"]["brightness"] | 255;

            pin2 = Jhead["amber"]["pin"];
            cur2 = Jhead["amber"]["current"];
            briA = Jhead["amber"]["brightness"] | 255;

            pin3 = Jhead["red"]["pin"];
            cur3 = Jhead["red"]["current"];
            briR = Jhead["red"]["brightness"] | 255;

            gar = new GARHEAD(&output1, pin1, pin2, pin3);
            gar->init(cur1, cur2, cur3);
            gar->setColorBrightness(green, briG);
            gar->setColorBrightness(amber, briA);
            gar->setColorBrightness(red, briR);

            heads[0].head = gar;
        }

        heads[0].destAddr = Jhead["destination"][0];

        if(heads[0].destAddr == 0)
        {
            gpio_put(GOODLED, HIGH);
            gpio_put(ERRORLED, LOW);
            panic("No Destination configured\n");
        }

        if(heads[0].destAddr == addr)
        {
            gpio_put(GOODLED, HIGH);
            gpio_put(ERRORLED, LOW);
            panic("Destination is this node");
        }

        heads[0].head->setHead(green);

        heads[0].dimBright = Jhead["dim"] | 255;

        heads[0].releaseTime = Jhead["release"] | 6;

        heads[0].delayClearStarted = false;
    }
    else
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        panic("No Head configured\n");
    }

    JsonObject cap = cfg["capture0"].as<JsonObject>();

    if(!cap.isNull())
    {
        uint8_t pin = cap["pin"];

        inputs[pin].mode = capture;
        inputs[pin].lastActive = false;
        inputs[pin].active = false;
        inputs[pin].headNum = cap["head1"];

        if(!cap["turnoutPin"].isNull())
        {
            inputs[pin].mode = turnoutCapture;
            inputs[pin].turnoutPinNum = cap["turnoutPin"];
            inputs[pin].headNum2 = cap["head2"];
        }
    }
    else
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        panic("No Capture Configured");
    }

    JsonObject rel = cfg["release0"].as<JsonObject>();

    if(!rel.isNull())
    {
        uint8_t pin = rel["pin"];

        inputs[pin].mode = release;
        inputs[pin].lastActive = false;
        inputs[pin].active = true;
        inputs[pin].headNum = 0;
    }
    else
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        panic("No Capture Configured");
    }
}

int main()
{
    //Initialize for printf
    stdio_init_all();

    //Initialize the input interrupt 
    gpio_init(SWITCHINT);
    gpio_set_dir(SWITCHINT, GPIO_IN);
    gpio_pull_up(SWITCHINT);

    //Initialize the RFM95 interrupt
    gpio_init(RADIOINT);
    gpio_set_dir(RADIOINT, GPIO_IN);

    //Initialize the Good stat LED
    gpio_init(GOODLED);
    gpio_set_dir(GOODLED, GPIO_OUT);
    gpio_put(GOODLED, LOW);

    //Initialize the Error stat LED
    gpio_init(ERRORLED);
    gpio_set_dir(ERRORLED, GPIO_OUT);
    gpio_put(ERRORLED, HIGH);

    //Initialize the on board LED 
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, HIGH);

    //Initialize the ADC for battery monitoring
    adc_init();
    adc_gpio_init(ADCIN);
    adc_select_input(0);

    //Setup GPIO interrupts and callback 
    gpio_set_irq_enabled_with_callback(RADIOINT, RFM95_INT_MODE, true, gpio_isr);
    //gpio_set_irq_enabled(SWITCHINT, PCA9674_INT_MODE, true);

    //Delay to allow serial monitor to connect
    sleep_ms(5000);

    printf("PICO SIGNAL TEST\n");

    printf("ADC: %d\n", adc_read());
    printf("Battery: %2.2FV\n", adc_read() * CONVERSION_FACTOR);

    //Start the SPI and i2c buses 
    initi2c0();
    initspi0();

    //Set all PCA9674 pins as inputs
    input1.inputMask(0xFF);
    
    //Initialize all LED pins 
    for(int i = 0; i < 16; i++)
    {
        output1.setLEDcurrent(i, 0);
        output1.setLEDbrightness(i, 0);
    }

    //Read config file from the micro SD card 
    parseConfig();

    printf("NODE: %d\nDEST: %d\n", addr, heads[0].destAddr);

    //Set up the RFM95 radio
    radio.init();
    radio.setLEDS(RXLED, TXLED);
    radio.setAddress(addr);
    //Preamble length: 8
    radio.setPreambleLength(8);
    //Center Frequency
    radio.setFrequency(915.0);
    //Set TX power to Max
    radio.setTxPower(20);
    //Set Bandwidth 500kHz
    radio.setSignalBandwidth(500000);
    //Set Coding Rate 4/5
    radio.setCodingRate(5);
    //Set Spreading Factor 8
    radio.setSpreadingFactor(8);
    //Accept all packets
    radio.setPromiscuous(true);

    radio.setModeRX();

    //radio.printRegisters();

    printf("Retries: %d\nRetry Time: %dms\nDim Time: %dmin\nSleep Time: %dmin\n", maxRetries, retryTime, dimTime, sleepTime);

    while(1)
    {
        retryTimeout = get_absolute_time();
        do
        {
            if(radio.available())
            {
                to = 0;
                from = 0;
                len = sizeof(transmission);

                radio.recv((uint8_t *)& transmission, &len, &from, &to);

                printf("\nREC: Len: %d To: %d From: %d RSSI:%d\n", len, to, from, radio.lastSNR());
                printf("Dest: %d Aspect: %c ACK: %d CODE: %d\n\n", transmission.destination, transmission.aspect, transmission.isACK, transmission.isCode);

                if(len == sizeof(transmission))
                {
                    bool headFound = false;
                    uint8_t headNum = 0;
                    for(int i = 0; i < MAXHEADS; i++)
                    {
                        if(from == heads[i].destAddr)
                        {
                            headNum = i;
                            headFound = true;
                        }
                    }

                    if(transmission.destination == addr && !transmission.isCode && headFound)
                    {
                        if(!headOn)
                        {
                            output1.wake();

                            for(int i = 0; i < MAXHEADS; i++)
                            {
                                if(heads[i].head)
                                {
                                    heads[i].head->setHeadBrightness(255);
                                }
                            }

                            headOn = true;
                            headDim = false;
                        }
                        if(headDim)
                        {
                            for(int i = 0; i < MAXHEADS; i++)
                            {
                                if(heads[i].head)
                                {
                                    heads[i].head->setHeadBrightness(255);
                                }
                            }

                            headDim = false;
                        }

                        switch(transmission.aspect)
                        {
                            case 'G':
                            case 'g':
                                if(!transmission.isACK)
                                {
                                    transmit(from, 'G', true, false);
                                }

                                if(heads[headNum].head->getColor() == amber && !heads[headNum].delayClearStarted)
                                {
                                    add_alarm_in_ms((clearDelayTime * 1000), delayedClear, &heads[headNum], true);
                                    heads[headNum].delayClearStarted = true;
                                }
                                else
                                {
                                    heads[headNum].head->setHead(green);
                                }

                                for(int i = 0; i < MAXINPUTS; i++)
                                {
                                    if(inputs[i].headNum == 0 && inputs[i].mode == release)
                                    {
                                        inputs[i].active = false;
                                        inputs[i].lastActive = false;
                                        break;
                                    }
                                }

                                retries = 0;
                                dimTimeout = get_absolute_time();
                                break;

                            case 'A':
                            case 'a':
                                if(!transmission.isACK)
                                {
                                    transmit(from, 'R', true, false);
                                }

                                if(heads[headNum].head->getColor() == green || heads[headNum].head->getColor() == amber || heads[headNum].head->getColor() == off)
                                {                                   
                                    heads[headNum].head->setHead(amber);

                                    for(int i = 0; i < MAXINPUTS; i++)
                                    {
                                        if(inputs[i].headNum == 0 && inputs[i].mode == capture)
                                        {
                                            inputs[i].active = false;
                                            inputs[i].lastActive = false;
                                            break;
                                        }
                                    }

                                    retries = 0;
                                    dimTimeout = get_absolute_time();
                                    heads[0].releaseTimer = get_absolute_time();
                                }
                                break;
                            
                            case 'R':
                            case 'r':
                                if(!transmission.isACK)
                                {
                                    transmit(from, 'A', true, false);
                                }
                                
                                heads[headNum].head->setHead(red);

                                for(int i = 0; i < MAXINPUTS; i++)
                                {
                                    if(inputs[i].headNum == 0 && inputs[i].mode == release)
                                    {
                                        inputs[i].active = false;
                                        inputs[i].lastActive = false;
                                    }

                                    if(inputs[i].headNum == 0 && inputs[i].mode == capture)
                                    {
                                        inputs[i].active = false;
                                        inputs[i].lastActive = false;
                                    }
                                }

                                retries = 0;
                                dimTimeout = get_absolute_time();

                                heads[0].releaseTimer = get_absolute_time();
                                break;
                        }
                    }
                }
            }
            tDiff = (absolute_time_diff_us(retryTimeout, get_absolute_time()) / 1000);//Get time difference and convert to ms
        } while ((retries > 0) && (tDiff < retryTime));
        
        if(retries > 10)
        {
            for(int i = 0; i < MAXINPUTS; i++)
            {
                inputs[i].active = false;
                inputs[i].lastActive = false;
            }

            retries = 0;
        }
        else
        {
            for(int i = 0; i < MAXINPUTS; i++)
            {
                if(inputs[i].lastActive)
                {
                    inputs[i].lastActive = false;
                }
            }
        }
        
        input1.updateInputs();

        for(int i = 0; i < MAXINPUTS; i++)
        {
            //Latch capture or release signals
            if(inputs[i].mode == capture)
            {                
                if(input1.getInput(i, false) == LOW && !inputs[i].serviced && (heads[inputs[i].headNum].head->getColor() == green || heads[inputs[i].headNum].head->getColor() == off))
                {
                    inputs[i].active = true;
                    //inputs[i].lastActive = false;

                    inputs[i].serviced = true;

                    if(!alarmSet)
                    {
                        gpio_put(PICO_DEFAULT_LED_PIN, LOW);
                        add_alarm_in_ms(50, turnOnStat, NULL, true);
                        alarmSet = true;
                    }
                }
            }
            else if(inputs[i].mode == release)
            {
                if(input1.getInput(i, false) == LOW && !inputs[i].serviced && (heads[inputs[i].headNum].head->getColor() == amber || heads[inputs[i].headNum].head->getColor() == red))
                {
                    inputs[i].active = true;
                    //inputs[i].lastActive = false;

                    inputs[i].serviced = true;

                    if(!alarmSet)
                    {
                        gpio_put(PICO_DEFAULT_LED_PIN, LOW);
                        add_alarm_in_ms(50, turnOnStat, NULL, true);
                        alarmSet = true;
                    }
                }
            }
            //get direct input for turnout monitoring
            else if(inputs[i].mode == turnout)
            {
                inputs[i].active = input1.getInput(i, false);
            }

            if(inputs[i].serviced && input1.getInput(i, false) == HIGH)
            {
                inputs[i].serviced = false;
            }
        }

        input1.inputMask(0xFF);

        for(int i = 0; i < MAXINPUTS; i++)
        {
            if(inputs[i].mode == capture || inputs[i].mode == turnoutCapture)
            {
                if(inputs[i].active && !inputs[i].lastActive)
                {
                    if(heads[inputs[i].headNum].head->getColor() == off)
                    {
                        heads[inputs[i].headNum].head->setHead(green);
                    }

                    if(inputs[i].mode == turnoutCapture && inputs[inputs[i].turnoutPinNum].active)
                    {
                        transmit(heads[inputs[i].headNum2].destAddr, 'R', false, false);
                    }
                    else
                    {
                        transmit(heads[inputs[i].headNum].destAddr, 'R', false, false);
                    }

                    retries++;

                    inputs[i].lastActive = true;

                    printf("Sending Capture %d\n", inputs[i].headNum);
                }
            }
            else if(inputs[i].mode == release)
            {
                if(inputs[i].active && !inputs[i].lastActive)
                {
                    transmit(heads[inputs[i].headNum].destAddr, 'G', false, false);
                    retries++;

                    inputs[i].lastActive = true;

                    printf("Sending Release %d\n", inputs[i].headNum);
                }
            }
        }

        for(int i = 0; i < MAXHEADS; i++)
        {
            if(heads[i].head)
            {
                if(heads[i].head->getColor() == red && heads[i].releaseTime > 0)
                {
                    tDiff = ((absolute_time_diff_us(heads[i].releaseTimer, get_absolute_time()))/60000000);

                    if(tDiff >= heads[i].releaseTime)
                    {
                        printf("Release Timeout Head %d\n", i);

                        for(int x = 0; x < MAXINPUTS; x++)
                        {
                            if(inputs[x].mode == release)
                            {
                                inputs[x].active = true;
                            }
                        }
                    }
                }
            }
        }

        if(dimTime > 0)
        {
            tDiff = ((absolute_time_diff_us(dimTimeout, get_absolute_time()))/60000000);//convert time difference from microseconds to minutes

            if(tDiff >= dimTime && headOn && !headDim)
            {
                for(int i = 0; i < MAXHEADS; i++)
                {
                    if(heads[i].head)
                    {
                        printf("Dimming Head %d\n", i);
                        heads[i].head->setHeadBrightness(heads[i].dimBright);
                    }
                }

                headDim = true;
            }
        }

        if(sleepTime > 0)
        {
            tDiff = ((absolute_time_diff_us(dimTimeout, get_absolute_time()))/60000000);//convert time difference from microseconds to minutes

            if(tDiff >= sleepTime && headOn)
            {
                for(int i = 0; i < MAXHEADS; i++)
                {
                    if(heads[i].head)
                    {
                        printf("Turning off Head %d\n", i);
                        heads[i].head->setHead(off);
                    }
                }

                output1.sleep();

                headOn = false;
            }
        }
    }//end loop
}