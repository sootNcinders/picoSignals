#include "battery.h"
#include "main.h"
#include "heads.h"
#include "radio.h"
#include "led.h"

float Battery::battery = 0.0;
float Battery::lowBatThreshold = 11.75;
float Battery::lowBatReset = 12.1;
float Battery::lowBatShutdown = 10.0;
TaskHandle_t Battery::batteryHandle;

void Battery::init(void)
{
    adc_init();
    adc_gpio_init(ADCIN);
    adc_select_input(0);

    lowBatThreshold = Main::cfg["lowBattery"] | 11.75; //Voltage to activate low battery warning, 11.75v if not specified
    lowBatReset     = Main::cfg["batteryReset"] | 12.1; //Voltage to reset low battery warning, 12.1v if not specified
    lowBatShutdown  = Main::cfg["batteryShutdown"] | 10.0; //Voltage to shut down the system, 10.0v if not specified

    xTaskCreate(batteryTask, "Battery Task", 256, NULL, BATTERYPRIORITY, &batteryHandle);

    DPRINTF("Battery Task Initialized\n");
}

void Battery::batteryTask(void *pvParameters)
{
    uint16_t rawADC;
    float batt;
    float rawBat[NUMBATSAMPLES] = {0.0};

    bool batteryLow = false;
    bool lastLow = false;
    bool batteryShutdown = false;
    bool lastShutdown = false;
    bool blinkOff = false;

    while(true)
    {
        rawADC = adc_read();
        batt = (float)rawADC * CONVERSION_FACTOR;
        batt += DIODEDROP;

        for(int i = 0; i < NUMBATSAMPLES - 1; i++)
        {
            rawBat[i] = rawBat[i + 1];

            if(rawBat[i] == 0.0)
            {
                rawBat[i] = batt;
            }
        }
        rawBat[NUMBATSAMPLES - 1] = batt;

        batt = 0.0;
        for(int i = 0; i < NUMBATSAMPLES; i++)
        {
            batt += rawBat[i];
        }
        batt /= NUMBATSAMPLES;

        battery = batt;

        if(battery < lowBatThreshold && battery > 4.0)
        {
            batteryLow = true;
        }
        else if(battery > lowBatReset)
        {
            batteryLow = false;
        }

        if(batteryLow && blinkOff)
        {
            HEADS::dim();
            blinkOff = false;
        }
        else if(batteryLow && !blinkOff)
        {
            HEADS::sleep();
            blinkOff = true;
        }
        else if(!batteryLow && lastLow)
        {
            HEADS::wake();
        }
        lastLow = batteryLow;

        if(battery < lowBatShutdown && battery > 4.0)
        {
            batteryShutdown = true;
        }
        else
        {
            batteryShutdown = false;
        }

        if(batteryShutdown && !lastShutdown)
        {
            HEADS::sleep();
            Radio::sleep();
        }
        else if(!batteryShutdown && lastShutdown)
        {
            HEADS::wake();
            Radio::wake();
        }
        lastShutdown = batteryShutdown;

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

float Battery::getBatteryVoltage()
{
    return battery;
}