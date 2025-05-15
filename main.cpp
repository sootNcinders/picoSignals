#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/sync.h"
#include "pico/flash.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/adc.h"
#include "hardware/watchdog.h"
#include "hardware/flash.h"

#include "f_util.h"
#include "ff.h"
#include "sd_card.h"
#include "hw_config.h"
#include "ArduinoJson.h"

#include "battery.h"
#include "LED.h"
#include "radio.h"
#include "io.h"
#include "ctc.h"
#include "heads.h"
#include "menu.h"
#include "overlay.h"

JsonDocument Main::cfg = JsonDocument();
uint8_t* Main::flashJson = (uint8_t*) FLASHJSONADDR;

uint8_t Main::mode = STD;

int main(void)
{
    set_sys_clock_48mhz();

    //Initialize for printf
    stdio_init_all();

    sleep_ms(5000);

    DPRINTF("\n\nPico Signals V%dR%d\n\n", VERSION, REVISION);

    //Initialize LEDs first for error codes
    LED::init();

    //Load in config file, all following inits require config info
    Main::loadConfig();

    MENU::init();

    if(Main::mode == STD)
    {
        DPRINTF("\nStandard Signal Mode\n");
        Battery::init();
        IO::init();
        HEADS::init();
        CTC::init();
        Radio::init();

        if(!IO::post())
        {
            DPRINTF("Input Fault\n");
            LED::postLoop(BADINPUT);
        }
        else if(!HEADS::post())
        {
            DPRINTF("Output Fault\n");
            LED::postLoop(BADOUTPUT);
        }
    }
    else if(Main::mode == CTC)
    {
        DPRINTF("\nCTC Mode\n");
        Radio::init();
    }
    else if(Main::mode == OVL)
    {
        DPRINTF("\nOverlay Mode\n");
        Battery::init();
        IO::init();
        CTC::init();
        Radio::init();
        OVERLAY::init();

        if(!IO::post())
        {
            DPRINTF("Input Fault\n");
            LED::postLoop(BADINPUT);
        }
    }

    //Main::post();
    if(!Radio::post())
    {
        DPRINTF("Radio Fault\n");
        LED::postLoop(BADRADIO);
    }

    DPRINTF("Init complete\n");

    vTaskStartScheduler();
}

void Main::loadConfig(void)
{
    FIL file; //config file 
    FRESULT fr; //file system return results
    FATFS fs; //FAT file system interface
    DIR dir; //Directory of the file
    FILINFO fInfo; //Information on the file
    sd_card_t *pSD; //SD card driver pointer
    bool sdMounted = true;
    bool fileFound = true;

    flashJson = (uint8_t*) FLASHJSONADDR;

    if(!sd_card_detect(0))
    {
        DPRINTF("No SD Card Detected\n");
    }
    
    //Get the SD card and start its driver
    pSD = sd_get_by_num(0); 
    sd_init_driver();

    //Mount the SD card file system, if it fails set the error light and stop execution
    fr = f_mount(&fs, "0:", 1);
    if (fr != FR_OK)
    {
        sdMounted = false;
        LED::setError(SDMOUNT);
        //panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    //Locate the config file in the file system, fault out if it cant be found
    if(sdMounted)
    {
        fr = f_findfirst(&dir, &fInfo, "", "config*.json");
        if(fr != FR_OK)
        {
            fileFound = false;
            LED::setError(CONFIGREAD);
            //panic("f_findfirst error: %s (%d)\n", FRESULT_str(fr), fr);
        }
    }
    //Open the config file, if it fails set the error light and stop execution
    //const char* const filename = "config.json";
    if(sdMounted && fileFound)
    {
        const char* const filename = fInfo.fname;
        fr = f_open(&file, filename, FA_READ);
        if (fr != FR_OK && fr != FR_EXIST)
        {
           LED::setError(CONFIGREAD);
            //panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
        }
    }

    //Read the config file into RAM, if it fails set error light and stop execution
    char cfgRaw[FILESIZE]; //May need to expand for more complex config files
    memcpy(cfgRaw, 0, sizeof(cfgRaw));
    UINT readSize = 0;
    if(sdMounted && fileFound)
    {
    fr = f_read(&file, &cfgRaw, sizeof(cfgRaw), &readSize);
    if(fr != FR_OK)
    {
        LED::setError(CONFIGREAD);
        //panic("f_read(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
        memcpy(cfgRaw, flashJson, sizeof(cfgRaw));
    }
    else
    {
        DPRINTF("Read %d characters of config file\n", readSize);
        //writeFlashJSON((uint8_t*)cfgRaw);

        //flash_safe_execute((void(*)(void*))writeFlashJSON, (void *)cfgRaw, 10000);
        writeFlashJSON((uint8_t*)cfgRaw);
    }
    }
    else
    {
        memcpy(cfgRaw, flashJson, sizeof(cfgRaw));
    }

    //Parse the JSON file into objects for easier handling, if it fails set the error light and stop execution
    DeserializationError error = deserializeJson(cfg, cfgRaw);
    if(error)
    {
        DPRINTF("Config JSON error: %s\n", error.c_str());
        LED::errorLoop(CONFIGREAD);
    }

    if(strncasecmp(cfg["mode"], "ctc", 3) == 0)
    {
        Main::mode = CTC;
    }
    else if(strncasecmp(cfg["mode"], "overlay", 7) == 0)
    {
        Main::mode = OVL;
    }
    else
    {
        Main::mode = STD;
    }
}

void Main::eraseFlashJSON(void)
{
    flash_range_erase((FLASHJSONADDR - XIP_BASE), FILESIZE);
}

/**
 * Writes the given JSON data to the flash memory if it is different from the
 * existing JSON data.
 *
 * @param in pointer to the JSON data to be written
 *
 * @throws None
 */
void Main::writeFlashJSON(uint8_t* in)
{
    bool irq[26];

    flashJson = (uint8_t*) FLASHJSONADDR;
    int cmp = memcmp(flashJson, in, FILESIZE);

    if(cmp != 0)
    {
        vTaskSuspendAll();

        for(uint8_t i = 0; i < sizeof(irq); i++)
        {
            irq[i] = irq_is_enabled(i);
            irq_set_enabled(i, false);
        }

        eraseFlashJSON();

        watchdog_update();
        flash_range_program((FLASHJSONADDR - XIP_BASE), in, FILESIZE);

        for(uint8_t i = 0; i < sizeof(irq); i++)
        {
            irq_set_enabled(i, irq[i]);
        }

        xTaskResumeAll();

        DPRINTF("JSON written to flash\n");
    }
    else
    {
        DPRINTF("JSON matched, not written\n");
    }
}

void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName )
{
    panic("Stack overflow in task %s\n", pcTaskName);
}

void vApplicationMallocFailedHook( void )
{
    panic("Failed to allocate memory\n");
}

void Main::reset(void)
{
    DPRINTF("Reboot\n");

    vTaskSuspendAll();

    watchdog_enable(500, true);

    while(1)
    {
        tight_loop_contents();
    }

    xTaskResumeAll();
}

void Main::post()
{
    bool ledState = HIGH;
    if(!IO::post())
    {
        DPRINTF("Input Fault\n");
        LED::postLoop(BADINPUT);
    }
    else if(!HEADS::post())
    {
        DPRINTF("Output Fault\n");
        LED::postLoop(BADOUTPUT);
    }
    else if(!Radio::post())
    {
        DPRINTF("Radio Fault\n");
        LED::postLoop(BADRADIO);
    }
}
