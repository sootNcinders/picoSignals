#include "io.h"
#include "ctc.h"
#include "heads.h"

#include "hardware/i2c.h"

pca9674 IO::input = pca9674(i2c0, 0x20);
SemaphoreHandle_t IO::ioMutex;
switchInfo IO::inputs[MAXINPUTS];

uint8_t IO::ovlHeads = 0;

void IO::init() 
{
    JsonObject pins[] = {Main::cfg["pin1"].as<JsonObject>(), Main::cfg["pin2"].as<JsonObject>(), Main::cfg["pin3"].as<JsonObject>(), Main::cfg["pin4"].as<JsonObject>(),
                            Main::cfg["pin5"].as<JsonObject>(), Main::cfg["pin6"].as<JsonObject>(), Main::cfg["pin7"].as<JsonObject>(), Main::cfg["pin8"].as<JsonObject>()};

    ioMutex = xSemaphoreCreateMutex();

    i2c_init(i2c0, 1000 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    // Make the I2C pins available to picotool
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));

    //Set all PCA9674 pins as inputs
    input.inputMask(0xFF);

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
            else if(pins[i]["mode"] == "ovlGreen")
            {
                inputs[i].mode = ovlGreen;
                inputs[i].headNum = pins[i]["head"];

                if(inputs[i].headNum > ovlHeads)
                {
                    ovlHeads = inputs[i].headNum;
                }

                inputs[i].headNum--;
            }
            else if(pins[i]["mode"] == "ovlAmber")
            {
                inputs[i].mode = ovlAmber;
                inputs[i].headNum = pins[i]["head"];

                if(inputs[i].headNum > ovlHeads)
                {
                    ovlHeads = inputs[i].headNum;
                }

                inputs[i].headNum--;
            }
            else if(pins[i]["mode"] == "ovlRed")
            {
                inputs[i].mode = ovlRed;
                inputs[i].headNum = pins[i]["head"];

                if(inputs[i].headNum > ovlHeads)
                {
                    ovlHeads = inputs[i].headNum;
                }

                inputs[i].headNum--;
            }
            else if(pins[i]["mode"] == "ovlAuxIn")
            {
                inputs[i].mode = ovlAuxIn;
            }
            else
            {
                inputs[i].headNum = 0xFF;
                inputs[i].headNum2 = 0xFF;
            }

            inputs[i].active = inputs[i].lastActive = false;
        }
    }

    xTaskCreate(ioTask, "IO Task", 256, NULL, IOPRIORITY, NULL);

    DPRINTF("IO Task Initialized\n");
}

void IO::ioTask(void *pvParameters)
{
    while(true)
    {
        //DPRINTF("IO Task\n");

        xSemaphoreTake(ioMutex, portMAX_DELAY);
        input.updateInputs();
        xSemaphoreGive(ioMutex);

        for(int i = 0; i < MAXINPUTS; i++)
        {
            inputs[i].raw = input.getInput(i, false);

            if(inputs[i].raw != inputs[i].lastRaw)
            {
                inputs[i].lastChange = get_absolute_time();
            }

            inputs[i].lastRaw = inputs[i].raw;

            //Latch capture or release signals
            if(inputs[i].mode == capture || inputs[i].mode == release || inputs[i].mode == turnoutCapture)
            {
                //raw input must stay inactive for 500ms before the clear is latched
                if(inputs[i].lastActive && !inputs[i].raw && ((absolute_time_diff_us(inputs[i].lastChange, get_absolute_time())/1000) > /*50*/500))
                {
                    inputs[i].active = inputs[i].lastActive = false;
                }
                //raw input must stay active for 5ms before set is latched
                else if(!inputs[i].active && inputs[i].raw && ((absolute_time_diff_us(inputs[i].lastChange, get_absolute_time())/1000) > 5))
                {
                    inputs[i].active = true;

                    if(inputs[i].mode == turnoutCapture && inputs[inputs[i].turnoutPinNum].active)
                    {
                        HEADS::startComm(inputs[i].headNum2);
                    }
                    else
                    {
                        HEADS::startComm(inputs[i].headNum);
                    }
                }
            }
            //get direct input for turnout monitoring, input must be stable for 50ms to latch
            else if((inputs[i].mode == turnout || inputs[i].mode == ovlGreen || inputs[i].mode == ovlAmber || inputs[i].mode == ovlRed || inputs[i].mode == ovlAuxIn) 
                        && inputs[i].active != inputs[i].raw && ((absolute_time_diff_us(inputs[i].lastChange, get_absolute_time())/1000) > 50))
            {
                DPRINTF("Turnout %d = %d\n", i, inputs[i].raw);
                inputs[i].active = inputs[i].raw;
                CTC::update();
            }
            else if(inputs[i].mode == unused)
            {
                inputs[i].active = inputs[i].raw;
            }

            if(absolute_time_diff_us(inputs[i].lastChange, get_absolute_time()) < 0)
            {
                inputs[i].lastChange = get_absolute_time();
            }
        }

        input.inputMask(0xFF);

        vTaskDelay(5/portTICK_PERIOD_MS);
    }
}

SemaphoreHandle_t IO::getIOmutex()
{
    return ioMutex;
}

void IO::getSwitchInfo(switchInfo* info)
{
    memcpy(info, inputs, sizeof(switchInfo) * MAXINPUTS);
}

void IO::setCapture(uint8_t headNum)
{
    for(uint8_t i = 0; i < MAXINPUTS; i++)
    {
        if((inputs[i].mode == capture || inputs[i].mode == turnoutCapture) && inputs[i].headNum == headNum)
        {
            inputs[i].active = true;
            inputs[i].lastActive = false;
        }
    }
}

void IO::setRelease(uint8_t headNum)
{
    for(uint8_t i = 0; i < MAXINPUTS; i++)
    {
        if(inputs[i].mode == release && inputs[i].headNum == headNum)
        {
            inputs[i].active = true;
            inputs[i].lastActive = false;
        }
    }
}

void IO::setLastActive(uint8_t headNum, uint8_t mode)
{
    for(uint8_t i = 0; i < MAXINPUTS; i++)
    {
        if((inputs[i].headNum == headNum && inputs[i].mode == mode) || (inputs[i].mode == mode && mode == turnoutCapture && inputs[i].headNum2 == headNum))
        {
            inputs[i].lastActive = true;
        }
    }
}

bool IO::getCapture(uint8_t headNum)
{
    bool rtn = false;

    for(uint8_t i = 0; i < MAXINPUTS; i++)
    {
        if((inputs[i].mode == capture && inputs[i].headNum == headNum) 
            || (inputs[i].mode == turnoutCapture && ((inputs[i].headNum == headNum && !inputs[inputs[i].turnoutPinNum].active) 
                    || (inputs[i].headNum2 == headNum && inputs[inputs[i].turnoutPinNum].active))))
        {
            if(inputs[i].active && !inputs[i].lastActive)
            {
                rtn = true;
            }
        }
    }

    return rtn;
}

bool IO::getRelease(uint8_t headNum)
{
    bool rtn = false;

    for(uint8_t i = 0; i < MAXINPUTS; i++)
    {
        if(inputs[i].mode == release && inputs[i].headNum == headNum)
        {
            if(inputs[i].active && !inputs[i].lastActive)
            {
                rtn = true;
                break;
            }
        }
    }

    return rtn;
}

bool IO::getOvlG(uint8_t headNum)
{
    bool rtn = false;

    for(uint8_t i = 0; i < MAXINPUTS; i++)
    {
        if(inputs[i].mode == ovlGreen && inputs[i].headNum == headNum)
        {
            if(inputs[i].active && !inputs[i].lastActive)
            {
                rtn = true;
                break;
            }
        }
    }

    return rtn;
}

bool IO::getOvlA(uint8_t headNum)
{
    bool rtn = false;    

    for(uint8_t i = 0; i < MAXINPUTS; i++)
    {
        if(inputs[i].mode == ovlAmber && inputs[i].headNum == headNum)
        {
            if(inputs[i].active && !inputs[i].lastActive)
            {
                rtn = true;
                break;
            }
        }
    }

    return rtn;
}

bool IO::getOvlR(uint8_t headNum)
{
    bool rtn = false;    

    for(uint8_t i = 0; i < MAXINPUTS; i++)
    {
        if(inputs[i].mode == ovlRed && inputs[i].headNum == headNum)
        {
            if(inputs[i].active && !inputs[i].lastActive)
            {
                rtn = true;
                break;
            }
        }
    }

    return rtn;
}

uint8_t IO::getNumOvlHeads()
{
    return ovlHeads;
}

bool IO::post()
{
    bool rtn = false;

    if(input.inputMask(0xFF))
    {
        rtn = true;
    }

    return rtn;
}

uint8_t IO::getOvlAuxIn()
{
    uint8_t rtn = 0;
    uint8_t pos = 0;

    for(uint8_t i = 0; i < MAXINPUTS; i++)
    {
        if(inputs[i].mode == ovlAuxIn)
        {
            rtn |= (inputs[i].active << pos);
            pos++;
        }
    }

    return rtn;
}
