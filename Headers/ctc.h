#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "radio.h"

#ifndef CTC_H
#define CTC_H

#define MAXCTC 8
#define MAXTRIES 3

typedef struct
{
    uint8_t addr;
    uint8_t triesRemaining;
    bool    responded;
}CTCNODEINFO;

class CTC
{
    public:
        static void init(void);
        static void update(void);
        static void pause(bool on);
        static void processFromMsg(FROMCTC msg, uint8_t from);
        static void processToMsg(TOCTC msg);
        static TaskHandle_t getTaskHandle(void);

    private:
        static void ctcTask(void *pvParameters);

        static bool updateNeeded;
        static bool paused;
        static bool ovlMode;

        static TaskHandle_t ctcTaskHandle;
        static uint8_t addr;
        static uint8_t tries;
        static uint8_t maxTries;

        static CTCNODEINFO ctcNodes[8];
};

#endif
