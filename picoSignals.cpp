#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/multicore.h"
#include "pico/sync.h"
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

#define VERSION 2
#define REVISION 0

#define MAXHEADS 4
#define MAXINPUTS 8
#define MAXDESTINATIONS 6

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

#define BLINK_INTERVAL 500 //ms

//Battery voltage conversion factor
#define CONVERSION_FACTOR (12.0/1284) //((3.3f / 0xFFF) * 10)

#define DPRINTF(...){printf("[%07.3f] ", ((to_us_since_boot(get_absolute_time())%1000000)/1000.0));printf(__VA_ARGS__);}

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
    bool    active = false; //Current pin state
    bool    lastActive = false; //Previous pin state
    bool    raw = false;
    bool    lastRaw = false;
    absolute_time_t lastChange;
}switchInfo;

//signal head info
typedef struct
{
    Head    *head; //The head driver
    uint8_t numDest;
    uint8_t destAddr[MAXDESTINATIONS]; //this heads partner
    bool destResponded[MAXDESTINATIONS];
    uint8_t dimBright; //Dimmed brightness level
    uint8_t retries;
    uint8_t releaseTime; //Time to wait to release
    absolute_time_t releaseTimer; //Absolute time of capture
    bool delayClearStarted;
}headInfo;


//Packet Structure
typedef struct
{
  uint8_t destination; //Unsigned 8 bit integer destination address, 0-255
  bool isACK;  //Is this packet an acknowledgement
  bool isCode; //Is this packet a code control packet - Currently Unused. Left in for compatibility. May use for interblock comms 
  char aspect; //New aspect color for signal head. R - red, A - amber, G - green
} RCL;
RCL transmission; // The packet

typedef struct
{
    uint8_t  destination;
    uint16_t heads;
    uint8_t  inputs;
    uint16_t status;
} TOCTC;
TOCTC toCTC;

typedef struct
{
    uint8_t destination;
    uint8_t head;
    char    cmd;
} FROMCTC;
FROMCTC fromCTC;

uint8_t tempbuf[6];

//Message data
uint8_t to;
uint8_t from;
uint8_t len;

//This nodes address 
uint8_t addr = 0;

//CTC board updating variables, not currently used
bool    ctcPresent = false;
bool    changed = false;
uint8_t ctcAddr = 254;
uint8_t ctcRetries = 0;
bool    ctcSending = false;

//Input pin states
switchInfo inputs[MAXINPUTS];

//IC drivers
pca9674 input1(i2c0, 0x20);
PCA9955 output1(i2c0, 0x01, 57.375);
RFM95 radio(spi0, PICO_DEFAULT_SPI_CSN_PIN, RADIOINT, 0);

//array of info for each head
headInfo heads[MAXHEADS];

bool headOn = true; //is any head on
bool headDim = false; //are all heads dim

//Variables loaded from the config file
uint8_t maxRetries = 10; //maximum number of tries to send a packet
uint8_t retryTime = 100;//time between attempts in milliseconds
uint8_t dimTime = 10; //time before dimming the head in minutes
uint8_t sleepTime = 15;//time before putting the signal heads in minutes
uint8_t clearDelayTime = 10;//time before letting amber go to green in seconds, used to help traffic flow alternate
float   lowBatThreshold = 11.0; //Voltage to trigger low battery warning
float   lowBatReset = 12.0; //Voltage to reset low battery warning

absolute_time_t retryTimeout; //abolute time of last packet send attempt
absolute_time_t dimTimeout; //absolute time of last time the signal head changed colors
absolute_time_t blinkTimer; //absolute time for the head blinking and battery checking

uint8_t blinkCounter = 0; //Counter for blinking the heads
bool batteryLow = false; //low battery flag
bool blinkOff = false; //current head state for blinking, used to prevent reset leaving the head off
float bat = 0.0; //measured battery voltage

volatile bool alarmSet = false; //flag so only one stat light timer can be set at a time, prevents overloading the hardware timers

bool statState = false; //Current state of the stat light
absolute_time_t statBlink; //Absolute time of last change of stat light

critical_section_t i2cCS; //Mutex for i2c bus, only used for multicore 

//alarm call back function to set the head to green from amber after the delay
int64_t delayedClear(alarm_id_t id, void *user_data)
{
    if(user_data)
    {
        if(((headInfo*)user_data)->head->getColor() != red) //stop the head from clearing if the block was captured before the timer finished
        {
            DPRINTF("Delayed clear\n");
            ((headInfo*)user_data)->head->setHead(green);
            changed = true;
        }
        ((headInfo*)user_data)->delayClearStarted = false;
    }

    return 0;
}

//Interrupt Service Routine for all GPIO pins, handles interrupt signals from the radio transciever
void gpio_isr(uint gpio, uint32_t event_mask)
{
    //printf("ISR GPIO: %d MASK: %d\n", gpio, event_mask);

    switch(gpio)
    {
        //Interrupt from the radio
        case RADIOINT:
            //if(event_mask == RFM95_INT_MODE)
            {
                radio.handleInterrupt();
            }
            break;

        //Interrupt from the input chip, not currently used
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

bool sendError = false;
//transmit a certain packet out
//Transmit time: 2ms
//Response time: 47mS
void transmit(uint8_t dest, char asp, bool ack, bool code)
{
    //Pack the data packet
    transmission.destination = dest;
    transmission.aspect = asp;
    transmission.isACK = ack;
    transmission.isCode = code;

    //If the transmission fails for any reason, set the error light
    if(!radio.send(255, (uint8_t*) &transmission, sizeof(transmission)))
    {
        //if the send failed set the error light
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        sendError = true;
    }
    //If the radio recovers, clear the error light
    else if(sendError)
    {
        //if the transciever driver recovers, clear the error
        gpio_put(GOODLED, LOW);
        gpio_put(ERRORLED, HIGH);
        sendError = false;
    }
}

//Function to handle when the transmission sequence is complete for multiple destinations
void setLastActive(uint8_t hN, uint8_t f, uint8_t m)
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
        for(int i = 0; i < MAXINPUTS; i++)
        {
            if((inputs[i].headNum == hN && inputs[i].mode == m) || (inputs[i].mode == turnoutCapture && inputs[i].headNum2 == hN  && inputs[inputs[i].turnoutPinNum].active))
            {
                inputs[i].lastActive = true;
                heads[i].retries = 0;
            }
        }
        for(int x = 0; x < heads[hN].numDest; x++)
        {
            heads[hN].destResponded[x] = false;
        }
    }
}

#ifdef MULTICORE
void core1loop()
{
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
                bool localRelease = false;

                radio.recv((uint8_t *)& transmission, &len, &from, &to);

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
                        switch(transmission.aspect)
                        {
                            case 'G':
                            case 'g':
                                if(!transmission.isACK)
                                {
                                    transmit(from, 'G', true, false);
                                }
                                
                                for(int i = 0; i < MAXINPUTS; i++)
                                {
                                    if(inputs[i].headNum == headNum && inputs[i].mode == release)
                                    {
                                        localRelease = inputs[i].active;
                                        inputs[i].lastActive = true;
                                        break;
                                    }
                                }

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
                                    changed = true;
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

                                    changed = true;

                                    for(int i = 0; i < MAXINPUTS; i++)
                                    {
                                        if(inputs[i].headNum == headNum && inputs[i].mode == capture)
                                        {
                                            inputs[i].lastActive = true;
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

                                changed = true;

                                for(int i = 0; i < MAXINPUTS; i++)
                                {
                                    if(inputs[i].headNum == headNum && inputs[i].mode == release)
                                    {
                                        inputs[i].lastActive = true;
                                    }

                                    if(inputs[i].headNum == headNum && inputs[i].mode == capture)
                                    {
                                        inputs[i].lastActive = true;
                                    }
                                }

                                retries = 0;
                                dimTimeout = get_absolute_time();

                                heads[0].releaseTimer = get_absolute_time();
                                break;
                        }
                    }

                    if(transmission.aspect == 'A' || transmission.aspect == 'a' || transmission.aspect == 'R' || transmission.aspect == 'r')
                    {
                        if(!headOn)
                        {
                            output1.wake();

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

                            headOn = true;
                            headDim = false;
                        }
                        else if(headDim)
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
                    }
                }

                DPRINTF("REC: Len: %d To: %d From: %d RSSI:%d\n", len, to, from, radio.lastSNR());
                DPRINTF("Dest: %d Aspect: %c ACK: %d CODE: %d\n\n", transmission.destination, transmission.aspect, transmission.isACK, transmission.isCode);
            }
        } while ((retries > 0) && ((absolute_time_diff_us(retryTimeout, get_absolute_time()) / 1000) <= retryTime));
    }
}
#endif

//config JSON file parser
void parseConfig()
{
    FIL file; //config file 
    FRESULT fr; //file system return results
    FATFS fs; //FAT file system interface
    DIR dir; //Directory of the file
    FILINFO fInfo; //Information on the file
    sd_card_t *pSD; //SD card driver pointer

    if(!sd_card_detect(0))
    {
        DPRINTF("No SD Card Detected\n");
    }
    
    //Get the SD card and start its driver
    pSD = sd_get_by_num(0); 
    sd_init_driver();

    //Mount the SD card file system, if it fails set the error light and stop execution
    fr = f_mount(&fs, "0:", 1);
    if (fr != FR_OK)
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    //Locate the config file in the file system, fault out if it cant be found
    fr = f_findfirst(&dir, &fInfo, "", "config*.json");
    if(fr != FR_OK)
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        panic("f_findfirst error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    //Open the config file, if it fails set the error light and stop execution
    //const char* const filename = "config.json";
    const char* const filename = fInfo.fname;
    fr = f_open(&file, filename, FA_READ);
    if (fr != FR_OK && fr != FR_EXIST)
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    }

#define FILESIZE 10240
    //Read the config file into RAM, if it fails set error light and stop execution
    char cfgRaw[FILESIZE]; //May need to expand for more complex config files
    UINT readSize = 0;
    fr = f_read(&file, &cfgRaw, sizeof(cfgRaw), &readSize);
    if(fr != FR_OK)
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        panic("f_read(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    }
    DPRINTF("Read %d characters of config file\n", readSize);

    //Parse the JSON file into objects for easier handling, if it fails set the error light and stop execution
    StaticJsonDocument<FILESIZE> cfg;
    DeserializationError error = deserializeJson(cfg, cfgRaw);
    if(error)
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        panic("Config JSON error: %s\n", error.c_str());
    }

    addr = cfg["address"]; //Address of this node

    maxRetries = cfg["retries"] | 10; //Number of retries, default to 10 if not specified 

    retryTime = cfg["retryTime"] | 150; //Time to wait for a response, default to 150ms if not specified 

    dimTime = cfg["dimTime"] | 15; //Time to dim the head(s), default to 15min if not specified

    sleepTime = cfg["sleepTime"] | 30; //Time to put the heads to sleep, 30min if not specified

    lowBatThreshold = cfg["lowBattery"] | 11.0; //Voltage to activate low battery warning, 11v if not specified
    lowBatReset     = cfg["batteryReset"] | 12.0; //Voltage to reset low battery warning, 12v if not specified

    //address 0 is not used and node can not join the network without an ID
    if(addr == 0)
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        panic("Invalid Address\n");
    }

    uint8_t pin1, pin2, pin3, pin4, cur1, cur2, cur3, cur4 = 255;

    JsonObject Jhead[] = {cfg["head0"].as<JsonObject>(), cfg["head1"].as<JsonObject>(), cfg["head2"].as<JsonObject>(), cfg["head3"].as<JsonObject>()};

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
                cur1 = Jhead[i]["red"]["current"];

                pin2 = Jhead[i]["green"]["pin"];
                cur2 = Jhead[i]["green"]["current"];

                pin3 = Jhead[i]["blue"]["pin"];
                cur3 = Jhead[i]["blue"]["current"];

                heads[i].head = new RGBHEAD(&output1, pin1, pin2, pin3);
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
                    cur4 = Jhead[i]["lunar"]["current"];
                    briL = Jhead[i]["lunar"]["brightness"] | 255;
                }
                else
                {
                    pin4 = 255;
                }

                heads[i].head = new GARHEAD(&output1, pin1, pin2, pin3, pin4);
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
                gpio_put(GOODLED, HIGH);
                gpio_put(ERRORLED, LOW);
                panic("No Destination configured\n");
            }

            heads[i].dimBright = Jhead[i]["dim"] | 255; //Set the dim brightness value, default to 255 (full brightness) if not specified 

            heads[i].releaseTime = Jhead[i]["release"] | 6; //Set the time to auto clear the head, default to 6min if not specified

            heads[i].delayClearStarted = false;
        }
    }

    JsonObject pins[] = {cfg["pin0"].as<JsonObject>(), cfg["pin1"].as<JsonObject>(), cfg["pin2"].as<JsonObject>(), cfg["pin3"].as<JsonObject>(),
                            cfg["pin4"].as<JsonObject>(), cfg["pin5"].as<JsonObject>(), cfg["pin6"].as<JsonObject>(), cfg["pin7"].as<JsonObject>()};

    //Load the mode for up to 8 inputs
    for(int i = 0; i < MAXINPUTS; i++)
    {
        if(!pins[i].isNull())
        {
            if(pins[i]["mode"] == "capture")
            {
                inputs[i].mode = capture;
                inputs[i].headNum = pins[i]["head1"];

                if(!pins[i]["head2"].isNull())
                {
                    inputs[i].mode = turnoutCapture;
                    inputs[i].headNum2 = pins[i]["head2"];
                    inputs[i].turnoutPinNum = pins[i]["turnout"];
                }
            }
            else if(pins[i]["mode"] == "release")
            {
                inputs[i].mode = release;
                inputs[i].headNum = pins[i]["head"];
            }
            else if (pins[i]["mode"] == "turnout")
            {
                inputs[i].mode = turnout;
            }

            inputs[i].active = inputs[i].lastActive = false;
        }
    }
}

float checkBattery()
{
    uint16_t rawADC;
    float bat;

    rawADC = adc_read();
    bat = (float)rawADC * CONVERSION_FACTOR;

    bat = round(bat * 10) / 10;

    return bat;
}

int main()
{
    set_sys_clock_48mhz();

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
    statBlink = get_absolute_time();
    statState = true;

    //Initialize the ADC for battery monitoring
    adc_init();
    adc_gpio_init(ADCIN);
    adc_select_input(0);

    //Setup GPIO interrupts and callback 
    gpio_set_irq_enabled_with_callback(RADIOINT, RFM95_INT_MODE, true, gpio_isr);
    //gpio_set_irq_enabled(SWITCHINT, PCA9674_INT_MODE, true);

    //Delay to allow serial monitor to connect
    sleep_ms(5000);

    //print the current version and revision
    DPRINTF("PICO SIGNAL V%dR%d\n", VERSION, REVISION);

    //Read battery ADC - Test code
    uint16_t rawADC = adc_read();
    bat = checkBattery();
    DPRINTF("ADC: %d\n", rawADC);
    DPRINTF("Battery: %2.1FV\n\n", bat);

    if(bat < 8 && bat > 4)
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        panic("Low Battery! %2.1f\n", bat);
    }

    #ifdef MULTICORE
    critical_section_init(&i2cCS);
    input1.setCriticalSection(&i2cCS);
    output1.setCriticalSection(&i2cCS);
    #endif

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

    DPRINTF("\nNODE: %d\n", addr);

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
        sleep_ms(1000);
    }

    //Set up the RFM95 radio
    //12500bps datarate
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
    //Spreading Factor of 10 gives 3906bps - ~130ms round trip - Too slow?
    //Spreading Factor of 9 gives  7031bps - ~70ms round trip
    //Spreading Factor of 8 gives 12500bps - ~45ms round trip
    //Spreading Factor of 7 gives 21875bps - ~24ms round trip ****Insufficient Range****
    radio.setSpreadingFactor(9);
    //Accept all packets
    radio.setPromiscuous(true);

    radio.setModeRX();

    //radio.printRegisters();

    #ifdef MULTICORE
    multicore_launch_core1(core1loop);
    #endif

    /*for(int i = 0; i < MAXHEADS; i++)
    {
        if(heads[i].head)
        {
            transmit(heads[i].destAddr, 'G', false, false);
        }
    }*/

    //Set all heads to red to send releases so the block starts on clear.
    for(int i = 0; i < MAXHEADS; i++)
    {
        if(heads[i].head)
        {
            heads[i].head->setHead(red);
        }
    }
    for(int i = 0; i < MAXINPUTS; i++)
    {
        if(inputs[i].mode == release)
        {
            inputs[i].active = true;
        }
    }

    DPRINTF("Retries: %d\nRetry Time: %dms\nDim Time: %dmin\nSleep Time: %dmin\n", maxRetries, retryTime, dimTime, sleepTime);

    while(1)
    {
        #ifdef MULTICORE
        //Hold checking IO during message transmision and response
        absolute_time_t retryTimeoutd = get_absolute_time();
        while ((retries > 0) && ((absolute_time_diff_us(retryTimeoutd, get_absolute_time()) / 1000) <= retryTime));
        #else

        //Check to see if any head is currently transmitting
        uint8_t anyRetries = 0;
        for(int i = 0; i < MAXHEADS; i++)
        {
            if(heads[i].retries != 0)
            {
                anyRetries = heads[i].retries;
            }
        }
        if(ctcRetries != 0)
        {
            anyRetries = ctcRetries;
        }
        retryTimeout = get_absolute_time();
        do
        {
            //Check if any packets have come in
            if(radio.available())
            {
                to = 0;
                from = 0;
                len = sizeof(tempbuf);
                bool localRelease = false;

                //Load the packet into the correct data structure
                radio.recv((uint8_t *)& tempbuf, &len, &from, &to);

                //Do not process if the correct size packet was read from the radio
                if(len == sizeof(transmission))
                {
                    memcpy((void *) &transmission, tempbuf, sizeof(transmission));
                    //Find the head the corresponding to the transmitting node
                    bool headFound = false;
                    uint8_t headNum = 0;
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
                    if(transmission.destination == addr && !transmission.isCode && headFound && from != addr)
                    {
                        switch(transmission.aspect)
                        {
                            case 'G':
                            case 'g':
                                //Respond if necessary 
                                if(!transmission.isACK)
                                {
                                    transmit(from, 'G', true, false);
                                }
                                else
                                {
                                    //Check if the release was from this node
                                    for(int i = 0; i < MAXINPUTS; i++)
                                    {
                                        if(inputs[i].headNum == headNum && inputs[i].mode == release)
                                        {
                                            localRelease = inputs[i].active;
                                            break;
                                        }
                                    }
                                }

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
                                    changed = true;
                                }

                                if(!headOn)
                                {
                                    output1.wake();

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

                                    headOn = true;
                                    headDim = false;
                                }
                                else if(headDim)
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

                                heads[headNum].retries = 0;
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

                                    changed = true;

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
                                    for(int i = 0; i < MAXINPUTS; i++)
                                    {
                                        if((inputs[i].headNum == headNum || (inputs[i].headNum2 == headNum && inputs[inputs[i].turnoutPinNum].active)) 
                                            && (inputs[i].mode == capture || inputs[i].mode == turnoutCapture))
                                        {
                                            if(inputs[i].active && !inputs[i].lastActive)
                                            {
                                                lostRace = true;
                                            }
                                        }
                                    }
                                }
                                
                                if((heads[headNum].head->getColor() != amber || heads[headNum].delayClearStarted) && !lostRace)
                                {
                                    if(!transmission.isACK)
                                    {
                                        transmit(from, 'A', true, false);
                                    }
                                    
                                    heads[headNum].head->setHead(red);

                                    changed = true;

                                    for(int i = 0; i < MAXINPUTS; i++)
                                    {
                                        if(inputs[i].headNum == headNum && inputs[i].mode == release)
                                        {
                                            inputs[i].lastActive = true;
                                        }

                                        if((inputs[i].headNum == headNum || (inputs[i].headNum2 == headNum && inputs[inputs[i].turnoutPinNum].active)) 
                                            && (inputs[i].mode == capture || inputs[i].mode == turnoutCapture))
                                        {
                                            inputs[i].lastActive = true;
                                        }
                                    }

                                    heads[headNum].retries = 0;
                                    dimTimeout = get_absolute_time();

                                    heads[headNum].releaseTimer = get_absolute_time();
                                }
                                else
                                {
                                    transmit(from, 'R', true, false);
                                }
                                break;
                        }
                    }

                    if(transmission.aspect == 'A' || transmission.aspect == 'a' || transmission.aspect == 'R' || transmission.aspect == 'r')
                    {
                        if(!headOn)
                        {
                            output1.wake();

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

                            headOn = true;
                            headDim = false;
                            changed = true;
                        }
                        else if(headDim)
                        {
                            for(int i = 0; i < MAXHEADS; i++)
                            {
                                if(heads[i].head)
                                {
                                    heads[i].head->setHeadBrightness(255);
                                }
                            }

                            headDim = false;
                            changed = true;
                        }

                        dimTimeout = get_absolute_time();
                    }
                }
                else if(len == sizeof(fromCTC) && from == ctcAddr)
                {
                    memcpy((void *)& fromCTC, tempbuf, sizeof(fromCTC));
                    if(fromCTC.destination == addr)
                    {
                        switch(fromCTC.cmd)
                        {
                            case 'W':
                                if(!headOn)
                                {
                                    output1.wake();

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

                                    headOn = true;
                                    headDim = false;
                                }
                                else if(headDim)
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

                                dimTimeout = get_absolute_time();
                                changed = true;
                                break;

                            case 'U':
                                changed = true;
                                break;

                            case 'C':
                                for(int i = 0; i < MAXINPUTS; i++)
                                {
                                    if((inputs[i].mode == capture || inputs[i].mode == turnoutCapture) && 
                                        (inputs[i].headNum == fromCTC.head || inputs[i].headNum2 == fromCTC.head))
                                    {
                                        inputs[i].active = true;
                                        inputs[i].lastActive = false;
                                        heads[fromCTC.head].retries = 0;
                                    }
                                }
                                break;

                            case 'R':
                                for(int i = 0; i < MAXINPUTS; i++)
                                {
                                    if(inputs[i].mode == release && inputs[i].headNum == fromCTC.head)
                                    {
                                        inputs[i].active = true;
                                        inputs[i].lastActive = false;
                                        heads[fromCTC.head].retries = 0;
                                    }
                                }
                                break;
                        }
                    }
                }
                else if(len == 1 && from == ctcAddr)
                {
                    ctcRetries = 0;
                    ctcSending = false;
                    changed = false;
                }

                //Possibly adjust delay time each time a packet is received

                DPRINTF("REC: Len: %d To: %d From: %d RSSI:%d\n", len, to, from, radio.lastSNR());
                DPRINTF("Dest: %d Aspect: %c ACK: %d CODE: %d\n\n", transmission.destination, transmission.aspect, transmission.isACK, transmission.isCode);
            }
        } while ((anyRetries > 0) && ((absolute_time_diff_us(retryTimeout, get_absolute_time()) / 1000) <= retryTime));
        #endif

        if((absolute_time_diff_us(statBlink, get_absolute_time()) / 1000) >= 1000)
        {
            statState = !statState;
            gpio_put(PICO_DEFAULT_LED_PIN, statState);
            statBlink = get_absolute_time();
        }
        
        for(int i = 0; i < MAXHEADS; i++)
        {
            if(heads[i].retries > 10)
            {
                if(heads[i].destResponded[0])
                {
                    for(int x = 0; x < heads[i].numDest; x++)
                    {
                        if(heads[i].destResponded[x] == false)
                        {
                            heads[i].destResponded[x] = true;
                            break;
                        }
                    }

                    bool missingResponse = false;
                    for(int x = 0; x < heads[i].numDest; x++)
                    {
                        if(heads[i].destResponded[x] == false)
                        {
                            missingResponse = true;
                            heads[i].retries = 0;
                        }
                    }
                    if(!missingResponse)
                    {
                        for(int x = 0; x < MAXINPUTS; x++)
                        {
                            if((inputs[x].headNum == i || (inputs[x].mode == turnoutCapture && inputs[x].headNum2 == i)) && inputs[x].active)
                            {
                                inputs[x].lastActive = true;
                                heads[i].retries = 0;
                                //break;
                            }
                        }
                        for(int x = 0; x < heads[i].numDest; x++)
                        {
                            heads[i].destResponded[x] = false;
                        }
                    }
                }
                else
                {
                    for(int x = 0; x < MAXINPUTS; x++)
                    {
                        if((inputs[x].headNum == i || (inputs[x].mode == turnoutCapture && inputs[x].headNum2 == i)) && inputs[x].active)
                        {
                            inputs[x].lastActive = true;
                            heads[i].retries = 0;
                            //break;
                        }
                    }
                    for(int x = 0; x < heads[i].numDest; x++)
                    {
                        heads[i].destResponded[x] = false;
                    }
                }
            }
        }
        
        input1.updateInputs();

        for(int i = 0; i < MAXINPUTS; i++)
        {
            inputs[i].raw = input1.getInput(i, false);

            if(inputs[i].raw != inputs[i].lastRaw)
            {
                inputs[i].lastChange = get_absolute_time();
            }

            inputs[i].lastRaw = inputs[i].raw;

            //Latch capture or release signals
            if(inputs[i].mode == capture || inputs[i].mode == release || inputs[i].mode == turnoutCapture)
            {
                //raw input must stay inactive for 2500ms before the clear is latched
                if((/*inputs[i].active ||*/ inputs[i].lastActive) && !inputs[i].raw && (absolute_time_diff_us(inputs[i].lastChange, get_absolute_time()) > (/*50*/2500*1000)))
                {
                    inputs[i].active = inputs[i].lastActive = false;
                }
                //raw input must stay active for 5ms before set is latched
                else if(!inputs[i].active && inputs[i].raw && (absolute_time_diff_us(inputs[i].lastChange, get_absolute_time()) > (5*1000)))
                {
                    inputs[i].active = true;

                    if(inputs[i].mode == turnoutCapture && inputs[inputs[i].turnoutPinNum].active)
                    {
                        for(int x = 0; x < MAXDESTINATIONS; x++)
                        {
                            heads[inputs[i].headNum2].destResponded[x] = false;
                        }
                        heads[inputs[i].headNum2].retries = 0;
                    }
                    else
                    {
                        for(int x = 0; x < MAXDESTINATIONS; x++)
                        {
                            heads[inputs[i].headNum].destResponded[x] = false;
                        }
                        heads[inputs[i].headNum].retries = 0;
                    }
                }
            }
            //get direct input for turnout monitoring, input must be stable for 50ms to latch
            else if(inputs[i].mode == turnout && inputs[i].active != inputs[i].raw && (absolute_time_diff_us(inputs[i].lastChange, get_absolute_time()) > (50*1000)))
            {
                printf("Turnout %d = %d\n", i, inputs[i].raw);
                inputs[i].active = inputs[i].raw;
                changed = true;
            }
        }

        input1.inputMask(0xFF);

        for(int i = 0; i < MAXINPUTS; i++)
        {
            if(inputs[i].mode == capture || inputs[i].mode == turnoutCapture)
            {
                if(inputs[i].active && !inputs[i].lastActive)
                {
                    if(!headOn)
                    {
                        output1.wake();

                        for(int i = 0; i < MAXHEADS; i++)
                        {
                            if(heads[i].head && heads[i].head->getColor() == off)
                            {
                                heads[i].head->setHeadBrightness(255);
                                heads[i].head->setHead(green);
                            }
                        }

                        headOn = true;
                        dimTimeout = get_absolute_time();
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
                        dimTimeout = get_absolute_time();
                    }

                    if(inputs[i].mode == turnoutCapture && inputs[inputs[i].turnoutPinNum].active)
                    {
                        bool incompleteSend = false;
                        bool transmitted = false;
                        for(int x = 0; x < heads[inputs[i].headNum2].numDest; x++)
                        {
                            if(!heads[inputs[i].headNum2].destResponded[x] && heads[inputs[i].headNum2].destResponded[0])
                            {
                                incompleteSend = true;
                            }
                        }
                        if(heads[inputs[i].headNum2].head->getColor() == green || incompleteSend)
                        {
                            for(int x = 0; x < heads[inputs[i].headNum2].numDest; x++)
                            {
                                if(!heads[inputs[i].headNum2].destResponded[x])
                                {
                                    transmit(heads[inputs[i].headNum2].destAddr[x], 'R', false, false);
                                    heads[inputs[i].headNum2].retries++;
                                    DPRINTF("Sending Capture %d to %d\n", inputs[i].headNum2, heads[inputs[i].headNum2].destAddr[x]);
                                    transmitted = true;
                                    break;
                                }
                            }

                            for(int x = 0; x < MAXINPUTS; x++)
                            {
                                if(inputs[x].mode == release && inputs[i].headNum2 == inputs[x].headNum)
                                {
                                    inputs[x].lastActive = true;
                                }
                            }
                        }
                        if(!transmitted)
                        {
                            inputs[i].lastActive = true;
                        }
                        else
                        {
                            break;    
                        }
                    }
                    else
                    {
                        bool incompleteSend = false;
                        bool transmitted = false;
                        for(int x = 0; x < heads[inputs[i].headNum].numDest; x++)
                        {
                            if(!heads[inputs[i].headNum].destResponded[x] && heads[inputs[i].headNum].destResponded[0])
                            {
                                incompleteSend = true;
                            }
                        }
                        if(heads[inputs[i].headNum].head->getColor() == green || incompleteSend)
                        {
                            for(int x = 0; x < heads[inputs[i].headNum].numDest; x++)
                            {
                                if(!heads[inputs[i].headNum].destResponded[x])
                                {
                                    transmit(heads[inputs[i].headNum].destAddr[x], 'R', false, false);
                                    heads[inputs[i].headNum].retries++;
                                    DPRINTF("Sending Capture %d to %d\n", inputs[i].headNum, heads[inputs[i].headNum].destAddr[x]);
                                    transmitted = true;
                                    break;
                                }
                            }

                            for(int x = 0; x < MAXINPUTS; x++)
                            {
                                if(inputs[x].mode == release && inputs[i].headNum == inputs[x].headNum)
                                {
                                    inputs[x].lastActive = true;
                                }
                            }
                        }
                        if(!transmitted)
                        {
                            inputs[i].lastActive = true;
                        }
                        else
                        {
                            break;
                        }
                    }

                }
            }
            else if(inputs[i].mode == release)
            {
                bool incompleteSend = false;
                bool transmitted = false;
                for(int x = 0; x < heads[inputs[i].headNum].numDest; x++)
                {
                    if(!heads[inputs[i].headNum].destResponded[x] && heads[inputs[i].headNum].destResponded[0])
                    {
                        incompleteSend = true;
                    }
                }
                if(inputs[i].active && !inputs[i].lastActive && ((heads[inputs[i].headNum].head->getColor() == amber || heads[inputs[i].headNum].head->getColor() == red || incompleteSend)))
                {
                    for(int x = 0; x < heads[inputs[i].headNum].numDest; x++)
                    {
                        if(!heads[inputs[i].headNum].destResponded[x])
                        {
                            transmit(heads[inputs[i].headNum].destAddr[x], 'G', false, false);
                            heads[inputs[i].headNum].retries++;
                            DPRINTF("Sending Release %d to %d\n", inputs[i].headNum, heads[inputs[i].headNum].destAddr[x]);
                            transmitted = true;
                            break;
                        }
                    }

                    for(int x = 0; x < MAXINPUTS; x++)
                    {
                        if((inputs[x].mode == capture || inputs[x].mode == turnoutCapture) && inputs[i].headNum == inputs[x].headNum)
                        {
                            inputs[x].lastActive = true;
                        }
                    }
                }
                if(!transmitted)
                {
                    //inputs[i].lastActive = true;
                }
                else
                {
                    break;
                }
            }
        }

        //Check if any of the heads need to be cleared
        for(int i = 0; i < MAXHEADS; i++)
        {
            if(heads[i].head)
            {
                if((heads[i].head->getColor() != green && heads[i].head->getColor() != off) && heads[i].releaseTime > 0)
                {
                    if(((absolute_time_diff_us(heads[i].releaseTimer, get_absolute_time()))/60000000) >= heads[i].releaseTime)
                    {
                        DPRINTF("Release Timeout Head %d\n", i);

                        heads[i].head->setHead(green);
                        heads[i].releaseTimer = get_absolute_time();
                    }
                }
            }
        }

        if(absolute_time_diff_us(blinkTimer, get_absolute_time()) / 1000 >= BLINK_INTERVAL)
        {
            bat = checkBattery();
            if(bat < lowBatThreshold)
            {
                batteryLow = true;
                headDim = true;
                DPRINTF("Battery: %2.1FV\n\n", bat);
            }
            else if(checkBattery() > lowBatReset)
            {
                batteryLow = false;
            }

            if(batteryLow && headOn)
            {
                if(blinkCounter == 0)
                {
                    for(int i = 0; i < MAXHEADS; i++)
                    {
                        if(heads[i].head)
                        {
                            heads[i].head->setHeadBrightness(0);
                        }
                    }

                    blinkOff = true;
                }
                else if(blinkCounter == 4)
                {
                    if(headDim)
                    {
                        for(int i = 0; i < MAXHEADS; i++)
                        {
                            if(heads[i].head)
                            {
                                heads[i].head->setHeadBrightness(heads[i].dimBright);
                            }
                        }
                    }
                    else
                    {
                        for(int i = 0; i < MAXHEADS; i++)
                        {
                            if(heads[i].head)
                            {
                                heads[i].head->setHeadBrightness(255);
                            }
                        }
                    }

                    blinkOff = false; 
                }

                blinkCounter++;

                if(blinkCounter >= 8)
                {
                    blinkCounter = 0; 
                }
            }
            else
            {
                blinkCounter = 0;

                if(blinkOff)
                {
                    if(headDim)
                    {
                        for(int i = 0; i < MAXHEADS; i++)
                        {
                            if(heads[i].head)
                            {
                                heads[i].head->setHeadBrightness(heads[i].dimBright);
                            }
                        }
                    }
                    else
                    {
                        for(int i = 0; i < MAXHEADS; i++)
                        {
                            if(heads[i].head)
                            {
                                heads[i].head->setHeadBrightness(255);
                            }
                        }
                    }
                    blinkOff = false;
                }
            }

            blinkTimer = get_absolute_time();
        }

        //If the dim time has an acceptable value and the timer has expired, dim the head to the specified value to save power
        if(dimTime > 0)
        {
            if(((absolute_time_diff_us(dimTimeout, get_absolute_time()))/60000000) >= dimTime && headOn && !headDim)
            {
                for(int i = 0; i < MAXHEADS; i++)
                {
                    if(heads[i].head)
                    {
                        DPRINTF("Dimming Head %d\n", i);
                        heads[i].head->setHeadBrightness(heads[i].dimBright);
                    }
                }

                headDim = true;
            }
        }

        //If the sleep time has an acceptable value and the timer has expired, turn the heads off and put the LED driver in sleep mode to save power
        if(sleepTime > 0)
        {
            if((((absolute_time_diff_us(dimTimeout, get_absolute_time()))/60000000) >= sleepTime) && headOn)
            {
                for(int i = 0; i < MAXHEADS; i++)
                {
                    if(heads[i].head)
                    {
                        DPRINTF("Turning off Head %d\n", i);
                        heads[i].head->setHead(off);
                    }
                }

                output1.sleep();

                headOn = false;
            }
        }

        if(ctcPresent && changed)
        {
            uint16_t tmp = 0;

            toCTC.destination = ctcAddr;
            
            toCTC.heads = 0;
            for(int i = MAXHEADS - 1; i >= 0; i--)
            {
                if(headDim && headOn)
                {
                    tmp = tmp + 0x1;
                }
                else if(!headOn)
                {
                    tmp = tmp + 0x2;
                }
                else if(batteryLow)
                {
                    tmp = tmp + 0x3;
                }
                else
                {
                    tmp = tmp + 0x0;
                }
                tmp = tmp << 2;

                if(heads[i].head->getColor() == green)
                {
                    tmp = tmp + 0x0;
                }
                else if(heads[i].head->getColor() == amber)
                {
                    tmp = tmp + 0x1;
                }
                else if(heads[i].head->getColor() == red)
                {
                    tmp = tmp + 0x2;
                }
                else if( heads[i].head->getColor() == lunar)
                {
                    tmp = tmp + 0x3;
                }

                if(i != 0)
                {
                    tmp = tmp << 4;
                }
            }
            toCTC.heads = tmp;

            tmp = 0;
            toCTC.inputs = 0;
            for(int i = 0; i < MAXINPUTS; i++)
            {
                if(inputs[i].active)
                {
                    tmp = 1 << i;
                }
                toCTC.inputs = toCTC.inputs + tmp;
                tmp = 0;
            }

            toCTC.status = 0;
            toCTC.status = (uint16_t)(checkBattery() * 10);

            radio.send(ctcAddr, (uint8_t *)& toCTC, sizeof(toCTC));

            ctcSending = true;
            ctcRetries++;
        }
    }//end loop
}
