#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "main.h"
#include "pca9674.h"

#ifndef IO_H
#define IO_H

//input type
typedef enum
{
    unused = 0, 
    capture, //Capture input
    turnoutCapture, //Capture input, target based on turnout state
    release, //Release input
    turnout //Turnout input
}switchMode;

static const char* switchModes[] = {"Unused  ", "Capture ", "Turnout Capture ", "Release ", "Turnout "};

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

class IO
{
    public:
        static void init(void);

        static SemaphoreHandle_t getIOmutex(void);

        static void getSwitchInfo(switchInfo* info);

        static void setCapture(uint8_t headNum);
        static void setRelease(uint8_t headNum);
        static bool getCapture(uint8_t headNum);
        static bool getRelease(uint8_t headNum);

        static void setLastActive(uint8_t headNum, uint8_t mode);

    private:
        static void ioTask(void *pvParameters);

        static SemaphoreHandle_t ioMutex;
        static pca9674 input;
        static switchInfo inputs[MAXINPUTS];
};

#endif
