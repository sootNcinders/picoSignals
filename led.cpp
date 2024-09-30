#include "LED.h"

#include "hardware/watchdog.h"

uint8_t LED::error = 0;
uint8_t LED::post = 0;

bool LED::firstPass = false;

TaskHandle_t LED::ledHandle;
TaskHandle_t LED::errorHandle;

//Initialize the on board Stat LED and create a task for it
void LED::init(void)
{
    firstPass = false;

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, HIGH);

    gpio_init(ERRORLED);
    gpio_set_dir(ERRORLED, GPIO_OUT);
    gpio_put(ERRORLED, HIGH);

    gpio_init(GOODLED);
    gpio_set_dir(GOODLED, GPIO_OUT);
    gpio_put(GOODLED, HIGH);

    xTaskCreate(ledTask, "LED Task", 128, NULL, LEDPRIORITY, &ledHandle);
    xTaskCreate(errorLEDtask, "Error LED Task", 256, NULL, ERRORPRIORITY, &errorHandle);

    DPRINTF("LED Task Initialized\n");
    DPRINTF("Error LED Task Initialized\n");
}

void LED::ledTask(void *pvParameters)
{
    //watchdog_enable(8000, true);
    while(true)
    {
        //DPRINTF("LED Task\n");

        if(!firstPass)
        {
            //watchdog_enable(8000, true);
            firstPass = true;
        }

        watchdog_update();

        gpio_put(PICO_DEFAULT_LED_PIN, HIGH);

        vTaskDelay(1000/portTICK_PERIOD_MS);

        watchdog_update();

        gpio_put(PICO_DEFAULT_LED_PIN, LOW);

        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}

void LED::errorLEDtask(void *pvParameters)
{
    uint8_t count = 0;

    while(true)
    {
        //DPRINTF("Error LED Task\n");

        if(error)
        {
            if(count < error)
            {
                gpio_put(GOODLED, HIGH);
                gpio_put(ERRORLED, HIGH);

                vTaskDelay(ERRORPERIOD/portTICK_PERIOD_MS);

                gpio_put(ERRORLED, LOW);

                vTaskDelay(ERRORPERIOD/portTICK_PERIOD_MS);
            }
            else
            {
                gpio_put(ERRORLED, HIGH);
                vTaskDelay(ERRORPERIOD/portTICK_PERIOD_MS);
            }
            count++;

            if(count >= error + 6)
            {
                count = 0;
            }
            
        }
        else if(post)
        {
            if(count < post)
            {
                gpio_put(GOODLED, HIGH);
                gpio_put(ERRORLED, HIGH);

                vTaskDelay(ERRORPERIOD/portTICK_PERIOD_MS);

                gpio_put(GOODLED, LOW);
                gpio_put(ERRORLED, LOW);

                vTaskDelay(ERRORPERIOD/portTICK_PERIOD_MS);
            }
            else
            {
                gpio_put(GOODLED, HIGH);
                gpio_put(ERRORLED, HIGH);
                vTaskDelay(ERRORPERIOD/portTICK_PERIOD_MS);
            }
            count++;

            if(count >= post + 6)
            {
                count = 0;
            }
        }
        else
        {
            count = 0;
            gpio_put(GOODLED, LOW);
            gpio_put(ERRORLED, HIGH);
            vTaskSuspend(NULL);
        }
    }
}

void LED::setError(uint8_t code)
{
    if(error == 0 || code == 0)
    {
        error = code;
        
        vTaskResume(errorHandle);
    }
}

void LED::setPOST(uint8_t code)
{
    post = code;
    
    vTaskResume(errorHandle);
}

void LED::errorLoop(uint8_t code)
{
    uint8_t count = 0;

    while(true)
    {
        if(count < code)
        {
            gpio_put(GOODLED, HIGH);
            gpio_put(ERRORLED, HIGH);
            busy_wait_ms(ERRORPERIOD);

            gpio_put(ERRORLED, LOW);
            busy_wait_ms(ERRORPERIOD);
        }
        else
        {
            gpio_put(ERRORLED, HIGH);
            busy_wait_ms(ERRORPERIOD);
        }
        count++;

        if(count >= code + 6)
        {
            count = 0;
        }
    }     
}

uint8_t LED::getError(void)
{
    return error;
}
