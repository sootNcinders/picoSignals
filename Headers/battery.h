#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "hardware/adc.h"

#ifndef BATTERY_H
#define BATTERY_H

//Battery voltage conversion factor
#define CONVERSION_FACTOR (11.2f/1269) //((3.3f / (1 << 12)) * 10) //(12.0/1284)
#define DIODEDROP 0.8f
#define NUMBATSAMPLES 10

class Battery
{
    public:
        static void init(void);
        static float getBatteryVoltage(void);

    private:
        static void batteryTask(void *pvParameters);

        static float battery;
        static float lowBatThreshold;
        static float lowBatReset;
        static float lowBatShutdown;
        static TaskHandle_t batteryHandle;
};

#endif
