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
#include "hardware/clocks.h"

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
uint32_t Main::debugFlags = PRINT_ALWAYS;

int main(void)
{
    set_sys_clock_48mhz();

    //Initialize for printf
    stdio_init_all();

    //sleep_ms(5000);

    DPRINTF(PRINT_ALWAYS, "\n\nPico Signals V%dR%d\n\n", VERSION, REVISION);

    //Initialize LEDs first for error codes
    LED::init();

    //Load in config file, all following inits require config info
    Main::loadConfig();

    MENU::init();
    Radio::init();

    if(Main::mode == STD)
    {
        DPRINTF(PRINT_ALWAYS, "\nStandard Signal Mode\n");
        Battery::init();
        IO::init();
        HEADS::init();
        CTC::init();
        
        if(!IO::post())
        {
            DPRINTF(PRINT_ERROR, "Input Fault\n");
            LED::postLoop(BADINPUT);
        }
        else if(!HEADS::post())
        {
            DPRINTF(PRINT_ERROR, "Output Fault\n");
            LED::postLoop(BADOUTPUT);
        }
    }
    else if(Main::mode == CTC)
    {
        DPRINTF(PRINT_ALWAYS, "\nCTC Mode\n");
    }
    else if(Main::mode == OVL)
    {
        DPRINTF(PRINT_ALWAYS, "\nOverlay Mode\n");
        Battery::init();
        IO::init();
        CTC::init();
        OVERLAY::init();

        if(!IO::post())
        {
            DPRINTF(PRINT_ERROR, "Input Fault\n");
            LED::postLoop(BADINPUT);
        }
    }

    //Main::post();
    if(!Radio::post())
    {
        DPRINTF(PRINT_ERROR, "Radio Fault\n");
        LED::postLoop(BADRADIO);
    }

    DPRINTF(PRINT_ALWAYS, "Init complete\n");

    vTaskStartScheduler();

    while(1);
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

    //Get the SD card and start its driver
    pSD = sd_get_by_num(0); 

    if(!sd_card_detect(pSD))
    {
        DPRINTF(PRINT_ERROR, "No SD Card Detected\n");
    }
    
    sd_init_driver();

    //Mount the SD card file system, if it fails set the error light and stop execution
    fr = f_mount(&fs, "0:", 1);
    if (fr != FR_OK)
    {
        sdMounted = false;
        LED::setError(SDMOUNT);
        DPRINTF(PRINT_ERROR, "Failed to mount SD card file system\n");
    }

    //Locate the config file in the file system, fault out if it cant be found
    if(sdMounted)
    {
        fr = f_findfirst(&dir, &fInfo, "", "config*.json");
        if(fr != FR_OK)
        {
            fileFound = false;
            LED::setError(CONFIGREAD);
            DPRINTF(PRINT_CONFIG, "Failed to find config file on SD card\n");
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
            DPRINTF(PRINT_CONFIG, "Failed to open config file %s\n", filename);
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
            DPRINTF(PRINT_CONFIG, "Read %d characters of config file\n", readSize);

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
        DPRINTF(PRINT_CONFIG, "Config JSON error: %s\n", error.c_str());
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

    f_unmount("0:");

    sdSafeState();
}

void Main::eraseFlashJSON(void)
{
    flash_range_erase((FLASHJSONADDR - XIP_BASE), FILESIZE);
}

void Main::writeJSON(uint8_t* in)
{
    flash_range_program((FLASHJSONADDR - XIP_BASE), in, FILESIZE);
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

        //eraseFlashJSON();
        flash_safe_execute((void(*)(void*))eraseFlashJSON, NULL, 2000);

        watchdog_update();
        //flash_range_program((FLASHJSONADDR - XIP_BASE), in, FILESIZE);
        flash_safe_execute((void(*)(void*))writeJSON, in, 2000);

        for(uint8_t i = 0; i < sizeof(irq); i++)
        {
            irq_set_enabled(i, irq[i]);
        }

        xTaskResumeAll();

        DPRINTF(PRINT_CONFIG, "JSON written to flash\n");
    }
    else
    {
        DPRINTF(PRINT_CONFIG, "JSON matched, not written\n");
    }
}

bool Main::writeSdJSON(uint8_t* in)
{
    FIL file; //config file 
    FRESULT fr; //file system return results
    FATFS fs; //FAT file system interface
    DIR dir; //Directory of the file
    FILINFO fInfo; //Information on the file
    sd_card_t *pSD; //SD card driver pointer
    bool sdMounted = true;
    bool fileFound = true;
    uint32_t fileSize = 0;

    while(in[fileSize] != 0 && fileSize < FILESIZE)
    {
        fileSize++;
    }

    //Get the SD card and start its driver
    pSD = sd_get_by_num(0); 

    if(!sd_card_detect(pSD))
    {
        DPRINTF(PRINT_ERROR, "No SD Card Detected\n");
    }
    
    sd_init_driver();

    watchdog_update();

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
            //LED::setError(CONFIGREAD);
        }

        //Open the config file, if it fails set the error light and stop execution
        //const char* const filename = "config.json";
        if(fileFound)
        {
            const char* const filename = fInfo.fname;
            //fr = f_open(&file, filename, FA_READ);
            fr = f_unlink(filename);
            if (fr != FR_OK && fr != FR_EXIST)
            {
               //LED::setError(CONFIGREAD);
            }
        }

        char filename[40];
        snprintf(filename, sizeof(filename), "config%d.json", (uint8_t)Main::cfg["address"]);

        fr = f_open(&file, filename, FA_CREATE_ALWAYS | FA_WRITE);

        if(fr == FR_OK)
        {
            fr = f_write(&file, in, fileSize, NULL);

            if(fr == FR_OK)
            {
                fr = f_close(&file);
            }
        }
    }

    f_unmount("0:");

    sdSafeState();

    return (fr == FR_OK);
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
    DPRINTF(PRINT_ALWAYS, "Reboot\n");

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
        DPRINTF(PRINT_ERROR, "Input Fault\n");
        LED::postLoop(BADINPUT);
    }
    else if(!HEADS::post())
    {
        DPRINTF(PRINT_ERROR, "Output Fault\n");
        LED::postLoop(BADOUTPUT);
    }
    else if(!Radio::post())
    {
        DPRINTF(PRINT_ERROR, "Radio Fault\n");
        LED::postLoop(BADRADIO);
    }
}

void Main::sdSafeState(void)
{
    //pull all pins low for storage and surge protection
    gpio_set_dir(12, GPIO_OUT); //miso
    gpio_set_dir(11, GPIO_OUT); //mosi
    gpio_set_dir(10, GPIO_OUT); //sck
    gpio_set_dir(13, GPIO_OUT); //ss
    gpio_put(12, 0);
    gpio_put(11, 0);
    gpio_put(10, 0);
    gpio_put(13, 0);
}

void Main::setDebugFlag(uint32_t flag)
{
    debugFlags |= flag;
}

void Main::clearDebugFlag(uint32_t flag)
{
    debugFlags &= ~flag;
}

uint32_t Main::getDebugFlags(void)
{
    return debugFlags;
}

void Main::debugPrintf(uint32_t flags, const char* format, ...)
{
    va_list args;
    va_start(args, format);

    if(!(debugFlags & PRINT_MENU))
    {
        debugFlags &= PRINT_MENU;
    }
    
    if(debugFlags & flags)
    {
        if(!(flags & PRINT_CTC || flags & PRINT_MENU || flags & PRINT_UPDATE || flags & PRINT_REMOTECLI))
        {
            printf("[%07.3f] ", ((to_us_since_boot(get_absolute_time())%1000000)/1000.0));
        }

        vprintf(format, args);
    }

    va_end(args);
}
