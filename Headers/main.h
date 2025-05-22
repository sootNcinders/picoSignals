#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/flash.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "sd_card.h"
#include "hw_config.h"
#include "ArduinoJson.h"

#ifndef MAIN_H
#define MAIN_H

#define VERSION 3
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

#define RFM95CS 17

#define GOODLED  6
#define ERRORLED 7
#define RXLED    8
#define TXLED    9
#define ADCIN    26

#define DPRINTF(...){printf("[%07.3f] ", ((to_us_since_boot(get_absolute_time())%1000000)/1000.0));printf(__VA_ARGS__);}
//#define DPRINTF(...){printf(__VA_ARGS__);}

#define BLINKTIME 20

#define ERRORPERIOD 250

//Error codes
#define NOERROR 0
#define DEADBATTERY 1
#define SDMOUNT 2
#define CONFIGREAD 3
#define CONFIGBAD 4
#define TRANSMISSIONFAIL 5
#define WATCHDOG 6

static const char* errorCodes[] = {"No Errors", "Low Battery", "SD Mount Failed", "Config Read Failed", "Config Bad", "Transmission Fail", "Watchdog Occured"};

//POST Codes
#define BADINPUT 1
#define BADOUTPUT 2
#define BADRADIO 3

#define FILESIZE (FLASH_SECTOR_SIZE*3)

#define FLASHJSONADDR ((XIP_BASE + PICO_FLASH_SIZE_BYTES) - FILESIZE)

#define ULONG_MAX 0xFFFFFFFF

//Thread Priority
typedef enum
{
    LEDPRIORITY = 1,
    ERRORPRIORITY,
    BATTERYPRIORITY,
    MENUPRIORITY,
    CTCPRIORITY,
    IOPRIORITY,
    HEADSPRIORITY,
    HEADSCOMMPRIORITY,
    RADIOPRIORITY = HEADSCOMMPRIORITY + MAXHEADS + 1,
    MAXPRIORITY
} priorities;

typedef enum
{
    STD = 0,
    CTC,
    OVL
}mode;

#if MAXPRIORITY >= configMAX_PRIORITIES
#error "Max priority is greater than configMAX_PRIORITIES"
#endif

class Main
{
    public:
        static void loadConfig(void);
        static void eraseFlashJSON(void);
        static void writeJSON(uint8_t* in);
        static void writeFlashJSON(uint8_t* in);
        static bool writeSdJSON(uint8_t* in);
        static void reset(void);
        static void post(void);

        static JsonDocument cfg;
        static uint8_t* flashJson;

        static uint8_t mode; 

};

#endif
