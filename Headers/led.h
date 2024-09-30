#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

#ifndef LED_H
#define LED_H

class LED 
{
    public:
        /// @brief Initialize LEDs and threads for LED blinking
        /// @param  None
        static void init(void);
        
        /// @brief Sets the Error code to blink
        /// @param code the number of the blink code
        static void setError(uint8_t code);

        /// @brief Sets the POST code to blink
        /// @param code the number of the blink code
        static void setPOST(uint8_t code);

        static void errorLoop(uint8_t code);

        static uint8_t getError(void);

    private:
        /// @brief The STAT LED Task
        /// @param pvParameters 
        static void ledTask(void *pvParameters);

        /// @brief The Error LED Task
        /// @param pvParameters 
        static void errorLEDtask(void *pvParameters);

        static uint8_t error;
        static uint8_t post;

        static bool firstPass;

        static TaskHandle_t ledHandle;
        static TaskHandle_t errorHandle;


        
};

#endif
