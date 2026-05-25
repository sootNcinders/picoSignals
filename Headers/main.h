#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/structs/watchdog.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "sd_card.h"
#include "hw_config.h"
#include "ArduinoJson.h"

#ifndef MAIN_H
#define MAIN_H

#define VERSION 4
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

#define RFM95CS 17

#define GOODLED  6
#define ERRORLED 7
#define RXLED    8
#define TXLED    9
#define ADCIN    26

#define DPRINTF(...) {Main::debugPrintf(__VA_ARGS__);}
//#define DPRINTF(...){printf("[%07.3f] ", ((to_us_since_boot(get_absolute_time())%1000000)/1000.0));printf(__VA_ARGS__);}
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

//Bitmask defines for debug printing
#define PRINT_ERROR     0x00000001
#define PRINT_ERROR_DEC "PRINT_ERROR"
#define PRINT_THREAD    0x00000002
#define PRINT_THREAD_DEC "PRINT_THREAD"
#define PRINT_CONFIG    0x00000004
#define PRINT_CONFIG_DEC "PRINT_CONFIG"
#define PRINT_RADIO     0x00000008
#define PRINT_RADIO_DEC "PRINT_RADIO"
#define PRINT_INPUTS    0x00000010
#define PRINT_INPUTS_DEC "PRINT_INPUTS"
#define PRINT_HEADS     0x00000020
#define PRINT_HEADS_DEC "PRINT_HEADS"
#define PRINT_CTC       0x00000040
#define PRINT_CTC_DEC   "PRINT_CTC"
#define PRINT_MENU      0x00000080
#define PRINT_MENU_DEC  "PRINT_MENU"
#define PRINT_UPDATE    0x00000100
#define PRINT_UPDATE_DEC "PRINT_UPDATE"
#define PRINT_REMOTECLI 0x00000200
#define PRINT_REMOTECLI_DEC "PRINT_REMOTECLI"

#define PRINT_ALWAYS    0xFFFFFFFF

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
        static void sdSafeState(void);
        static void debugPrintf(uint32_t flags, const char* format, ...);
        static void setDebugFlag(uint32_t flag);
        static void clearDebugFlag(uint32_t flag);
        static uint32_t getDebugFlags(void);

        static JsonDocument cfg;
        static uint8_t* flashJson;

        static uint8_t mode; 
        
    private:
        static uint32_t debugFlags;

};

#endif
