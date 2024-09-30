#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "main.h"
#include "radio.h"

#include "pca9955.h"
#include "head.h"
#include "GARhead.h"
#include "RGBhead.h"

#ifndef HEADS_H
#define HEADS_H

#define MAXHEADS 4
#define MAXDESTINATIONS 6

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

class HEADS
{
    public:
        static void init(void);

        static void processRxMsg(RCL msg, uint8_t from);

        static void wake(void);

        static void dim(void);

        static void sleep(void);

        static void startComm(uint8_t headNum);

        static char getHead(uint8_t headNum);

        static uint16_t getLEDErrors();

    private:
        static void headsTask(void *pvParameters);

        static void headCommTask(void *pvParameters);

        static void setLastActive(uint8_t hN, uint8_t f, uint8_t m);

        static int64_t blinkOn(alarm_id_t id, void *user_data);

        static int64_t delayedClear(alarm_id_t id, void *user_data);

        static PCA9955 output;
        static headInfo heads[MAXHEADS];

        static uint8_t maxRetries;
        static uint8_t retryTime;
        static uint8_t dimTime;
        static uint8_t sleepTime;
        static uint8_t clearDelayTime;
        static bool monLEDsOpen;
        static bool monLEDsShort;

        static bool headsOn;
        static bool headsDim;

        static uint8_t awakeIndicator;

        static absolute_time_t dimTimeout;

        static TaskHandle_t headsTaskHandle;

        static TaskHandle_t headCommTaskHandle[MAXHEADS];
};

#endif
