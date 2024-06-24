#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/sync.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/adc.h"
#include "hardware/watchdog.h"
#include "hardware/flash.h"
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
#include <tusb.h>

#define VERSION 2
#define REVISION 2

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

#define NUMSAMPLES (4)
#define ALPHA (2/(1+NUMSAMPLES))

//Battery voltage conversion factor
#define CONVERSION_FACTOR (12.0/1284) //((3.3f / 0xFFF) * 10)

#define DPRINTF(...){printf("[%07.3f] ", ((to_us_since_boot(get_absolute_time())%1000000)/1000.0));printf(__VA_ARGS__);}

#define FILESIZE (FLASH_SECTOR_SIZE*3)

#define FLASHJSONADDR ((XIP_BASE + PICO_FLASH_SIZE_BYTES) - FILESIZE)
uint8_t* flashJson = (uint8_t*) FLASHJSONADDR;

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
struct RCL
{
  uint8_t destination; //Unsigned 8 bit integer destination address, 0-255
  //uint8_t voltage; //Unsinged 8 bit integer battery voltage, 0-255 representing 00.0 - 25.5V
  bool isACK;  //Is this packet an acknowledgement
  bool isCode; //Is this packet a code control packet
  char aspect; /*New aspect color for signal head. 
                * R - red 
                * A - amber 
                * G - green
                */
};
RCL transmission; // The packet


/* Serial input for map:
: - Start Chararcter
00 - Node Address, 00-FF
O - Head 1 - O, G, A, R, L
O - Head 2
O - Head 3
O - Head 4
0 - Captures, 0-F
0 - Releases, 0-F
0 - Turnouts, 0-F
00 - absolute value of average RSSI to primary partner, 0-FF
00 - Voltage x10, 0-FF */
struct TOCTC
{
    uint8_t sender;
    char head1;
    char head2;
    char head3;
    char head4;
    uint8_t captures;
    uint8_t releases;
    uint8_t turnouts;
    int8_t avgRSSI;
    uint8_t voltage;
    uint16_t ledError;
    const uint8_t version = VERSION;
    const uint8_t revision = REVISION;
};
TOCTC toCTC;

struct FROMCTC
{
    uint8_t dest;
    uint8_t cmd;
};
FROMCTC fromCTC;

uint8_t buf[sizeof(toCTC)];

float avgRSSI = 0;

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
float   lowBatThreshold = 11.75; //Voltage to trigger low battery warning
float   lowBatReset = 12.1; //Voltage to reset low battery warning

absolute_time_t retryTimeout; //abolute time of last packet send attempt
absolute_time_t dimTimeout; //absolute time of last time the signal head changed colors
absolute_time_t blinkTimer; //absolute time for the head blinking and battery checking
absolute_time_t ctcTimer;

uint8_t blinkCounter = 0; //Counter for blinking the heads
bool batteryLow = false; //low battery flag
bool blinkOff = false; //current head state for blinking, used to prevent reset leaving the head off
float bat = 0.0; //measured battery voltage
float lastBat = 0.0;

volatile bool alarmSet = false; //flag so only one stat light timer can be set at a time, prevents overloading the hardware timers

bool statState = false; //Current state of the stat light
absolute_time_t statBlink; //Absolute time of last change of stat light

//CTC board updating variables, not currently used
bool changed = false;
bool ctcPresent = false;

char cin = 0;
char inBuf[255];
uint8_t bufIdx = 0;

uint8_t ctcTries = 0;

uint8_t awakeIndicator = 255;

bool monLEDs = false;

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

/**
 * Sends data to the CTC (Central Traffic Control) system.
 *
 * @param data The data to be sent to the CTC system.
 *
 * @throws None
 */
void sendToCTC(TOCTC data)
{
    DPRINTF("Sending To CTC\n");
    //If the transmission fails for any reason, set the error light
    if(!radio.send(255, (uint8_t*) &data, sizeof(data)))
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

/**
 * Sends data from the CTC (Central Traffic Control) system to a specified destination.
 *
 * @param data The data to be sent, containing the destination and command.
 *
 * @throws None
 */
void sendFromCTC(FROMCTC data)
{
    DPRINTF("Sending from CTC to %d cmd %d\n", data.dest, data.cmd);
    //If the transmission fails for any reason, set the error light
    if(!radio.send(255, (uint8_t*) &data, sizeof(data)))
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
            if((inputs[i].headNum == hN && inputs[i].mode == m) || (m == turnoutCapture && inputs[i].headNum2 == hN  && inputs[inputs[i].turnoutPinNum].active))
            {
                inputs[i].lastActive = true;
                heads[i].retries = 0;
                //break;
            }
        }
        for(int x = 0; x < heads[hN].numDest; x++)
        {
            heads[hN].destResponded[x] = false;
        }
    }
}

/**
 * Writes the given JSON data to the flash memory if it is different from the
 * existing JSON data.
 *
 * @param in pointer to the JSON data to be written
 *
 * @throws None
 */
void writeFlashJSON(uint8_t* in)
{
    flashJson = (uint8_t*) FLASHJSONADDR;
    if(memcmp(flashJson, in, FILESIZE) != 0)
    {
        flash_range_erase((FLASHJSONADDR - XIP_BASE), FILESIZE);
        flash_range_program((FLASHJSONADDR - XIP_BASE), in, FILESIZE);

        DPRINTF("JSON written to flash\n");
    }
    else
    {
        DPRINTF("JSON matched, not written\n");
    }
}


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
        //panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    //Locate the config file in the file system, fault out if it cant be found
    fr = f_findfirst(&dir, &fInfo, "", "config*.json");
    if(fr != FR_OK)
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        //panic("f_findfirst error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    //Open the config file, if it fails set the error light and stop execution
    //const char* const filename = "config.json";
    const char* const filename = fInfo.fname;
    fr = f_open(&file, filename, FA_READ);
    if (fr != FR_OK && fr != FR_EXIST)
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        //panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    }

    //Read the config file into RAM, if it fails set error light and stop execution
    char cfgRaw[FILESIZE]; //May need to expand for more complex config files
    UINT readSize = 0;
    fr = f_read(&file, &cfgRaw, sizeof(cfgRaw), &readSize);
    if(fr != FR_OK)
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        //panic("f_read(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
        memcpy(cfgRaw, flashJson, sizeof(cfgRaw));
    }
    else
    {
        DPRINTF("Read %d characters of config file\n", readSize);
        writeFlashJSON((uint8_t*)cfgRaw);
    }

    //Parse the JSON file into objects for easier handling, if it fails set the error light and stop execution
    JsonDocument cfg;
    DeserializationError error = deserializeJson(cfg, cfgRaw);
    if(error)
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        panic("Config JSON error: %s\n", error.c_str());
    }

    addr = cfg["address"]; //Address of this node

    maxRetries = cfg["retries"] | 10; //Number of retries, default to 10 if not specified 

    retryTime = cfg["retryTime"] | 100; //Time to wait for a response, default to 100ms if not specified 

    dimTime = cfg["dimTime"] | 15; //Time to dim the head(s), default to 15min if not specified

    sleepTime = cfg["sleepTime"] | 30; //Time to put the heads to sleep, 30min if not specified

    lowBatThreshold = cfg["lowBattery"] | 11.0; //Voltage to activate low battery warning, 11v if not specified
    lowBatReset     = cfg["batteryReset"] | 12.0; //Voltage to reset low battery warning, 12v if not specified

    ctcPresent = cfg["ctcPresent"];

    monLEDs = cfg["monitorLEDs"]|false;

    //address 0 is not used and node can not join the network without an ID
    if(addr == 0)
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        panic("Invalid Address\n");
    }

    uint8_t pin1, pin2, pin3, pin4, cur1, cur2, cur3, cur4 = 255;

    JsonObject Jhead[] = {cfg["head1"].as<JsonObject>(), cfg["head2"].as<JsonObject>(), cfg["head3"].as<JsonObject>(), cfg["head4"].as<JsonObject>()};

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

    if(!cfg["AwakePin"].isNull())
    {
        awakeIndicator = cfg["AwakePin"];
    }
    else
    {
        awakeIndicator = 255;
    }

    JsonObject pins[] = {cfg["pin1"].as<JsonObject>(), cfg["pin2"].as<JsonObject>(), cfg["pin3"].as<JsonObject>(), cfg["pin4"].as<JsonObject>(),
                            cfg["pin5"].as<JsonObject>(), cfg["pin6"].as<JsonObject>(), cfg["pin7"].as<JsonObject>(), cfg["pin8"].as<JsonObject>()};

    //Load the mode for up to 8 inputs
    for(int i = 0; i < MAXINPUTS; i++)
    {
        if(!pins[i].isNull())
        {
            if(pins[i]["mode"] == "capture")
            {
                inputs[i].mode = capture;
                inputs[i].headNum = pins[i]["head1"];
                inputs[i].headNum--;

                if(!pins[i]["head2"].isNull())
                {
                    inputs[i].mode = turnoutCapture;
                    inputs[i].headNum2 = pins[i]["head2"];
                    inputs[i].headNum2--;
                    inputs[i].turnoutPinNum = pins[i]["turnout"];
                    inputs[i].turnoutPinNum--;
                }
            }
            else if(pins[i]["mode"] == "release")
            {
                inputs[i].mode = release;
                inputs[i].headNum = pins[i]["head"];
                inputs[i].headNum--;
            }
            else if (pins[i]["mode"] == "turnout")
            {
                inputs[i].mode = turnout;
            }

            inputs[i].active = inputs[i].lastActive = false;
        }
    }
}

/**
 * Reads the raw ADC value from the battery and converts it to a battery voltage.
 *
 * @return The battery voltage as a floating-point number.
 */
float checkBattery()
{
    uint16_t rawADC;
    float bat;

    rawADC = adc_read();
    bat = (float)rawADC * CONVERSION_FACTOR;

    return bat;
}

/**
 * Processes the command received from the CTC.
 *
 * @param f the FROMCTC structure containing the command to be processed
 *
 * @throws None
 */
void processFromCTC(FROMCTC f)
{
    switch(f.cmd)
    {
        case 0:
            changed = false;
            ctcTries = 0;
            break;
        case 1:
            changed = true;
            ctcTries = 0;
            break;
        case 2:
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

                if(awakeIndicator < 16)
                {
                    output1.setLEDbrightness(awakeIndicator, 255);
                }

                changed = true;
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
            break;
        case 3:
        case 4:
        case 5:
        case 6:
            if(f.dest == addr)
            {
                for(uint8_t i = 0; i < MAXINPUTS; i++)
                {
                    if((inputs[i].mode == capture || inputs[i].mode == turnoutCapture) && inputs[i].headNum == (f.cmd - 3))
                    {
                        inputs[i].active = true;
                        inputs[i].lastActive = false;
                    }
                }
            }
            break;
        case 7:
        case 8:
        case 9:
        case 10:
            if(f.dest == addr)
            {
                for(uint8_t i = 0; i < MAXINPUTS; i++)
                {
                    if(inputs[i].mode == release && inputs[i].headNum == (f.cmd - 7))
                    {
                        inputs[i].active = true;
                        inputs[i].lastActive = false;
                    }
                }
            }
            break;
    }
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
    gpio_put(GOODLED, HIGH);

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

    watchdog_enable(10000, true); // Set watchdog up, will trip after 10 seconds

    //print the current version and revision
    DPRINTF("PICO SIGNAL V%dR%d\n", VERSION, REVISION);

    //Read battery ADC - Test code
    uint16_t rawADC = adc_read();
    bat = checkBattery();
    DPRINTF("ADC: %d\n", rawADC);
    DPRINTF("Battery: %2.2FV\n\n", bat);

    if(bat < 8.0 && bat > 3.5)
    {
        gpio_put(GOODLED, HIGH);
        gpio_put(ERRORLED, LOW);
        DPRINTF("Low Battery! %2.2f\n", bat);

        radio.setModeSleep();

        while(bat < lowBatReset)
        {
            sleep_ms(60000);
            bat = checkBattery();
        }
    }

    //Start the SPI and i2c buses 
    initi2c0();
    initspi0();

    //watchdog_enable(10000, true); // Set watchdog up, will trip after 10 seconds

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

    memset(&buf, 0, sizeof(buf));

    if(awakeIndicator < 16)
    {
        output1.setLEDcurrent(awakeIndicator, 58);
        output1.setLEDbrightness(awakeIndicator, 255);
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

    //Auto release if the watchdog did not reboot the controller. Otherwise assume most restrictive condition for safety 
    if(!watchdog_caused_reboot())
    {
        for(int i = 0; i < MAXINPUTS; i++)
        {
            if(inputs[i].mode == release)
            {
                inputs[i].active = true;
            }
        }
    }

    DPRINTF("Retries: %d\nRetry Time: %dms\nDim Time: %dmin\nSleep Time: %dmin\n", maxRetries, retryTime, dimTime, sleepTime);

    gpio_put(GOODLED,  LOW);
    gpio_put(ERRORLED, HIGH);

    while(1)
    {
        watchdog_update(); //kick the dog

        //Check to see if any head is currently transmitting
        uint8_t anyRetries = 0;
        for(int i = 0; i < MAXHEADS; i++)
        {
            if(heads[i].retries != 0)
            {
                anyRetries = heads[i].retries;
            }
        }
        retryTimeout = get_absolute_time();
        do
        {
            //Check if any packets have come in
            if(radio.available())
            {
                memset(&buf, 0, sizeof(buf));
                to = 0;
                from = 0;
                len = sizeof(buf);
                bool localRelease = false;

                //Load the packet into the correct data structure
                radio.recv((uint8_t *)& buf, &len, &from, &to);

                //Do not process if the correct size packet was read from the radio
                if(len == sizeof(transmission))
                {
                    memcpy(&transmission, &buf, sizeof(transmission));
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
                                //else
                                //{
                                    //Check if the release was from this node
                                    for(int i = 0; i < MAXINPUTS; i++)
                                    {
                                        if(inputs[i].headNum == headNum && inputs[i].mode == release)
                                        {
                                            localRelease = inputs[i].active;
                                            break;
                                        }
                                    }
                                //}

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

                                    if(awakeIndicator < 16)
                                    {
                                        output1.setLEDbrightness(awakeIndicator, 255);
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

                    if(transmission.aspect == 'G' || transmission.aspect == 'g' || transmission.aspect == 'A' || 
                        transmission.aspect == 'a' || transmission.aspect == 'R' || transmission.aspect == 'r')
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

                            if(awakeIndicator < 16)
                            {
                                output1.setLEDbrightness(awakeIndicator, 255);
                            }

                            changed = true;
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
                    }

                    DPRINTF("REC: Len: %d To: %d From: %d RSSI:%d\n", len, to, from, radio.lastSNR());
                    DPRINTF("Dest: %d Aspect: %c ACK: %d CODE: %d\n\n", transmission.destination, transmission.aspect, transmission.isACK, transmission.isCode);

                    printf(";%0x%0x%c%0x\n", from, transmission.destination, transmission.aspect, abs(radio.lastSNR()));
                }
                else if(len == sizeof(toCTC))
                {
                    memcpy(&toCTC, &buf, sizeof(toCTC));
                    if(toCTC.captures <= 0xF && toCTC.releases <= 0xF && toCTC.turnouts <= 0xF)
                    {
                        printf(":%x%x%c%c%c%c%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x\n", toCTC.sender>>4, toCTC.sender&0xF, toCTC.head1, toCTC.head2, toCTC.head3, toCTC.head4,
                                    toCTC.captures, toCTC.releases, toCTC.turnouts, abs(toCTC.avgRSSI)>>4, abs(toCTC.avgRSSI)&0xF, toCTC.voltage>>4, toCTC.voltage&0xF, 
                                    (toCTC.ledError>>12)&0xF, (toCTC.ledError>>8)&0xF, (toCTC.ledError>>4)&0xF, toCTC.ledError & 0xF,
                                    toCTC.version>>4, toCTC.version&0xF, toCTC.revision>>4, toCTC.revision&0xF);
                    }
                }
                else if(len == sizeof(fromCTC))
                {
                    memcpy(&fromCTC, &buf, sizeof(fromCTC));

                    if(fromCTC.dest == addr)
                    {
                        DPRINTF("From CTC cmd: %d\n", fromCTC.cmd);
                        processFromCTC(fromCTC);
                    }
                }

                //Possibly adjust delay time each time a packet is received

                if(from == heads[0].destAddr[0])
                {
                    if(avgRSSI == 0)
                    {
                        avgRSSI = radio.lastSNR();
                    }
                    else
                    {
                        avgRSSI = (radio.lastSNR() * ALPHA) + (avgRSSI * (1-ALPHA));
                    }
                }
            }
        } while ((anyRetries > 0) && ((absolute_time_diff_us(retryTimeout, get_absolute_time()) / 1000) <= retryTime));

        //blink the status LED
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

                        if(awakeIndicator < 16)
                        {
                            output1.setLEDbrightness(awakeIndicator, 255);
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
                                    changed = false;
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
                                    changed = false;
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
                            changed = false;
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
                DPRINTF("Battery: %2.2FV\n\n", bat);

                if(lowBatReset - 1 > 0)
                {
                    while(bat < (lowBatThreshold - 1))
                    {
                        for(int i = 0; i < MAXHEADS; i++)
                        {
                            if(heads[i].head)
                            {
                                heads[i].head->setHead(off);
                            }
                        }

                        radio.setModeSleep();

                        sleep_ms(60000);
                        bat = checkBattery();
                    }
                    radio.setModeRX();
                }
            }
            else if(bat > lowBatReset)
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
                    for(int i = 0; i < MAXHEADS; i++)
                    {
                        if(heads[i].head)
                        {
                            //Dim the head to save power while in low power mode
                            heads[i].head->setHeadBrightness(heads[i].dimBright);
                        }
                    }

                    blinkOff = false; 
                    headDim = true;
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

            if(bat - lastBat > 0.25 || lastBat - bat > 0.25)
            {
                changed = true;
                lastBat = bat;
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

                if(awakeIndicator < 16)
                {
                    output1.setLEDbrightness(awakeIndicator, 0);
                }

                output1.sleep();

                changed = true;
                headOn = false;
            }
        }

        if(ctcPresent && changed && (absolute_time_diff_us(ctcTimer, get_absolute_time()) > 1000000))
        {
            toCTC.sender = addr;
            
            if(heads[0].head)
            {
                switch(heads[0].head->getColor())
                {
                    case green:
                        toCTC.head1 = 'G';
                        break;
                    case amber:
                        toCTC.head1 = 'A';
                        break;
                    case red:
                        toCTC.head1 = 'R';
                        break;
                    case lunar:
                        toCTC.head1 = 'L';
                        break;
                    case off:
                    default:
                        toCTC.head1 = 'O';
                        break;
                }
            }
            else
            {
                toCTC.head1 = 'O';
            }

            if(heads[1].head)
            {
                switch(heads[1].head->getColor())
                {
                    case green:
                        toCTC.head2 = 'G';
                        break;
                    case amber:
                        toCTC.head2 = 'A';
                        break;
                    case red:
                        toCTC.head2 = 'R';
                        break;
                    case lunar:
                        toCTC.head2 = 'L';
                        break;
                    case off:
                    default:
                        toCTC.head2 = 'O';
                        break;
                }
            }
            else
            {
                toCTC.head2 = 'O';
            }

            if(heads[2].head)
            {
                switch(heads[2].head->getColor())
                {
                    case green:
                        toCTC.head3 = 'G';
                        break;
                    case amber:
                        toCTC.head3 = 'A';
                        break;
                    case red:
                        toCTC.head3 = 'R';
                        break;
                    case lunar:
                        toCTC.head3 = 'L';
                        break;
                    case off:
                    default:
                        toCTC.head3 = 'O';
                        break;
                }
            }
            else
            {
                toCTC.head3 = 'O';
            }

            if(heads[3].head)
            {
                switch(heads[3].head->getColor())
                {
                    case green:
                        toCTC.head4 = 'G';
                        break;
                    case amber:
                        toCTC.head4 = 'A';
                        break;
                    case red:
                        toCTC.head4 = 'R';
                        break;
                    case lunar:
                        toCTC.head4 = 'L';
                        break;
                    case off:
                    default:
                        toCTC.head4 = 'O';
                        break;
                }
            }
            else
            {
                toCTC.head4 = 'O';
            }

            toCTC.captures = 0;
            toCTC.releases = 0;
            toCTC.turnouts = 0;

            uint8_t numTurnouts = 0;

            for(uint8_t i = 0; i < 8; i++)
            {
                if((inputs[i].mode == capture || inputs[i].mode == turnoutCapture) && inputs[i].active)
                {
                    toCTC.captures |= (1 << inputs[i].headNum);
                }
                else if(inputs[i].mode == release && inputs[i].active)
                {
                    toCTC.releases |= (1 << inputs[i].headNum);
                }
                else if(inputs[i].mode == turnout && inputs[i].active)
                {
                    toCTC.turnouts |= (1 << numTurnouts);
                    numTurnouts++;
                }
            }

            toCTC.avgRSSI = (int8_t) avgRSSI;
            toCTC.voltage = (uint8_t) (checkBattery() * 10);

            toCTC.ledError = 0;
            output1.checkErrors();
            if(monLEDs)
            {
                for(uint8_t i = 0; i < 16; i++)
                {
                    if(output1.getError(i) != 0)
                    {
                        toCTC.ledError |= (1 << i);
                    }
                }
            }

            printf(":%x%x%c%c%c%c%x%x%x%x%x%x%x%x%x%x%x%x%x%x%x\n", toCTC.sender>>4, toCTC.sender&0xF, toCTC.head1, toCTC.head2, toCTC.head3, toCTC.head4,
                    toCTC.captures, toCTC.releases, toCTC.turnouts, abs(toCTC.avgRSSI)>>4, abs(toCTC.avgRSSI)&0xF, toCTC.voltage>>4, toCTC.voltage&0xF, 
                    (toCTC.ledError>>12)&0xF, (toCTC.ledError>>8)&0xF, (toCTC.ledError>>4)&0xF, toCTC.ledError & 0xF,
                    toCTC.version>>4, toCTC.version&0xF, toCTC.revision>>4, toCTC.revision&0xF);

            sendToCTC(toCTC);

            ctcTries++;

            if(ctcTries > maxRetries)
            {
                changed = false;
                ctcTries = 0;
            }

            ctcTimer = get_absolute_time();
        }

        cin = getchar_timeout_us(0);
        while(((cin >= '0' && cin <= '9') || (cin >='A' && cin <= 'F') || (cin >='a' && cin <= 'f') || cin==':') && bufIdx < sizeof(inBuf))
        {
            printf("cin = %c (0x%x)\n", cin, cin);
            inBuf[bufIdx] = cin;
            
            if(bufIdx > 0)
            {
                bufIdx++;
            }
            if(cin == ':')
            {
                bufIdx = 1;
            }
            else if(bufIdx == 5)
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
                    processFromCTC(fromCTC);

                    if(fromCTC.dest == RFM95_BROADCAST_ADDR)
                    {
                        sendFromCTC(fromCTC);
                    }
                }
                else
                {
                    sendFromCTC(fromCTC);
                }

                ctcPresent = true;
            }

            cin = getchar_timeout_us(100);
        }

        if(absolute_time_diff_us(retryTimeout, get_absolute_time()) < 0)
        {
            retryTimeout = get_absolute_time();
        }
        if(absolute_time_diff_us(dimTimeout, get_absolute_time()) < 0)
        {
            dimTimeout = get_absolute_time();
        }
        if(absolute_time_diff_us(blinkTimer, get_absolute_time()) < 0)
        {
            blinkTimer = get_absolute_time();
        }
        if(absolute_time_diff_us(ctcTimer, get_absolute_time()) < 0)
        {
            ctcTimer = get_absolute_time();
        }
    }//end loop
}
