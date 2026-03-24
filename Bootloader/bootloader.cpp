#include "bootloader.h"

uint8_t Bootloader::hexRecord[0x30];
uint8_t Bootloader::flashData[FLASH_WRITE_SIZE];

uint16_t Bootloader::fileChecksum = 0;
uint16_t Bootloader::validRecordNumber = 0;
uint16_t Bootloader::flashDataIndex = 0;

uint32_t Bootloader::extendedAddress = 0;
uint32_t Bootloader::flashAddress = XIP_BASE + BOOTLOADER_SIZE_BYTES;

critical_section_t Bootloader::critical_section;

RFM95 Bootloader::radio = RFM95(spi0, PICO_DEFAULT_SPI_CSN_PIN, RADIOINT, 0);

void gpio_isr(uint gpio, uint32_t event_mask)
{
    if(gpio == RADIOINT)
    {
        Bootloader::radio.handleInterrupt();        
    }

    gpio_acknowledge_irq(gpio, event_mask);
}

int main(void)
{
    set_sys_clock_48mhz();

    //Initialize for printf
    stdio_init_all();

    //sleep_ms(5000);

    printf("Pico Bootloader Started\r\n");

    printf("Update: %d Flash: 0x%X 0x%X 0x%X 0x%X\r\n", 
        Bootloader::checkForUpdate(),
        flash_target_contents[0],
        flash_target_contents[1],
        flash_target_contents[2],
        flash_target_contents[3]);

    if(!Bootloader::checkForUpdate() && 
                (flash_target_contents[0]==0xF2) &&
                (flash_target_contents[1]==0xEB) &&
                (flash_target_contents[2]==0x88) &&
                (flash_target_contents[3]==0x71))
    {
        //Set magic numbers in case app fails to start
        watchdog_hw->scratch[5] = BOOTLOADER_ENTRY_MAGIC;
        watchdog_hw->scratch[6] = ~BOOTLOADER_ENTRY_MAGIC;

        //Set watchdog to catch any failed starts, max delay of 8.388sec 
        watchdog_enable(8388, true);
        watchdog_update();

        Bootloader::jumpToApplication();
    }
    else
    {
        printf("Entering Bootloader Update Mode\r\n");
        gpio_init(ERRORLED);
        gpio_set_dir(ERRORLED, GPIO_OUT);
        gpio_put(ERRORLED, true);

        gpio_init(GOODLED);
        gpio_set_dir(GOODLED, GPIO_OUT);
        gpio_put(GOODLED, true);
        
        spi_init(spi0, 5000 * 1000);
        gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
        gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
        gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);

        // Make the SPI pins available to picotool
        bi_decl(bi_3pins_with_func(PICO_DEFAULT_SPI_RX_PIN, PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI));

        spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

        //Initialize the RFM95 interrupt
        gpio_init(RADIOINT);
        gpio_set_dir(RADIOINT, GPIO_IN);

        gpio_set_irq_enabled_with_callback(RADIOINT, RFM95_INT_MODE, true, gpio_isr);

        //Set up the RFM95 radio
        //12500bps datarate
        Bootloader::radio.init();
        Bootloader::radio.setLEDS(RXLED, TXLED);
        Bootloader::radio.setAddress(watchdog_hw->scratch[7]);
        //Preamble length: 8
        Bootloader::radio.setPreambleLength(8);
        //Center Frequency
        Bootloader::radio.setFrequency(915.0);
        //Set TX power to Max
        Bootloader::radio.setTxPower(20);
        //Set Bandwidth 500kHz
        Bootloader::radio.setSignalBandwidth(500000);
        //Set Coding Rate 4/5
        Bootloader::radio.setCodingRate(5);
        //Spreading Factor of 12 gives 1172bps - ~425ms round trip - Too Slow
        //Spreading Factor of 11 gives 2148bps - ~260ms round trip - Untested, Maybe too slow
        //Spreading Factor of 10 gives 3906bps - ~130ms round trip - Untested
        //Spreading Factor of 9 gives  7031bps - ~70ms round trip
        //Spreading Factor of 8 gives 12500bps - ~45ms round trip
        //Spreading Factor of 7 gives 21875bps - ~24ms round trip ****Insufficient Range****
        Bootloader::radio.setSpreadingFactor(9);
        //Accept all packets
        Bootloader::radio.setPromiscuous(true);

        Bootloader::radio.setCADTimeout(150/2);

        Bootloader::radio.setModeRX();

        Bootloader::doUpdate();
    }
}

void Bootloader::doUpdate(void)
{
    uint8_t txBuf[255];
    uint16_t rcvdRecord = waitForHexRecord(hexRecord);
    uint16_t tmpRcvdNum = 0;

    while(true)
    {
        if(rcvdRecord > 0 && rcvdRecord == validRecordNumber + 1)
        {
            validRecordNumber++;
            processHexRecord(hexRecord);
            printf(".");
        }
        else if(rcvdRecord > validRecordNumber + 1 || rcvdRecord == 0)
        {
            tmpRcvdNum = validRecordNumber + 1;

            txBuf[0] = '*';
            txBuf[1] = 'N'; //Nacknowledge
            txBuf[2] = (tmpRcvdNum >> 8) & 0xFF;
            txBuf[3] = tmpRcvdNum & 0xFF;

            radio.send(255, txBuf, 4);

            printf("?:%d\r\n", rcvdRecord);
        }

        memset(hexRecord, 0, sizeof(hexRecord));
        rcvdRecord = waitForHexRecord(hexRecord);
    }
}

bool Bootloader::checkForUpdate(void)
{
    bool wd_says_so = (watchdog_hw->scratch[5] == BOOTLOADER_ENTRY_MAGIC) &&
		(watchdog_hw->scratch[6] == ~BOOTLOADER_ENTRY_MAGIC);

    return wd_says_so;
}

void Bootloader::jumpToApplication(void)
{
    printf("Jumping to Application at 0x%X\r\n", (XIP_BASE + BOOTLOADER_SIZE_BYTES));

    sleep_ms(100);

    // In an assembly snippet . . .
    // Set VTOR register, set stack pointer, and jump to reset
    asm volatile (
    "mov r0, %[start]\n"
    "ldr r1, =%[vtable]\n"
    "str r0, [r1]\n"
    "ldmia r0, {r0, r1}\n"
    "msr msp, r0\n"
    "bx r1\n"
    :
    : [start] "r" (XIP_BASE + BOOTLOADER_SIZE_BYTES), [vtable] "X" (PPB_BASE + M0PLUS_VTOR_OFFSET)
    :
    );
}

uint16_t Bootloader::waitForHexRecord(uint8_t* pHexRecord)
{
    uint8_t rxBuf[255];
    uint8_t txBuf[255];
    uint8_t len;
    uint8_t bufIndex = 0;
    uint8_t recordIndex = 0;
    uint8_t recordState = INTEL_HEX_START_STATE;
    uint8_t recordLength = 0;
    uint8_t recordLenCntr = 0;
    uint8_t csum = 0;
    uint8_t from;
    uint8_t to;
    uint16_t rcvdRecordNumber = 0;
    uint16_t tmpChecksum = 0;
    bool recordComplete = false;

    //Morse 'Z'
    static const bool ledState[] = {true, true, true, false, true, true, true, false, true, false, true, false, false, false};
    static uint8_t ledIndex = 0;
    static uint8_t ledTimer = 0;

    while(!recordComplete)
    {
        //Blink LEDs
        if(ledTimer++ >= 40)
        {
            ledTimer = 0;
            gpio_put(GOODLED, !ledState[ledIndex]);
            gpio_put(ERRORLED, !ledState[ledIndex]);
            ledIndex = (ledIndex + 1) % sizeof(ledState);
        }

        //Process incoming messages
        while(radio.available())
        {
            len = sizeof(rxBuf);
            radio.recv(rxBuf, &len, &from, &to);

            //printf("RCVD: %c%c%c%c\r\n", rxBuf[0], rxBuf[1], rxBuf[2], rxBuf[3]);
            
            //Bootloader message
            if(rxBuf[0] == '*')
            {
                switch(rxBuf[1])
                {
                    //Erase Command
                    case 'E':
                        if(rxBuf[2] >= 'A')
                        {
                            to = ((rxBuf[2] - (uint8_t)'A') + 10) << 4;
                        }
                        else
                        {
                            to = (rxBuf[2] - (uint8_t)'0') << 4;
                        }
                        if(rxBuf[3] >= 'A')
                        {
                            to += ((rxBuf[3] - (uint8_t)'A') + 10) << 4;
                        }
                        else
                        {
                            to += (rxBuf[3] - (uint8_t)'0') << 4;
                        }

                        if(to == watchdog_hw->scratch[7] || to == 255)
                        {
                            printf("Erasing Flash\r\n");

                            critical_section_enter_blocking(&critical_section);
                            flash_range_erase(BOOTLOADER_SIZE_BYTES, FLASH_SIZE_BYTES - BOOTLOADER_SIZE_BYTES - 12288);
                            critical_section_exit(&critical_section);

                            txBuf[0] = '*';
                            txBuf[1] = 'A'; //Acknowledge
                            radio.send(255, txBuf, 2);

                            //If update was interrupted, reset variables
                            fileChecksum = 0;
                            validRecordNumber = 0;
                            flashDataIndex = 0;
                            flashAddress = XIP_BASE + BOOTLOADER_SIZE_BYTES;
                            recordState = INTEL_HEX_START_STATE;
                        }
                        break;

                    //File Checksum
                    case 'C':
                        //Dump any remaining data
                        if(flashDataIndex > 0)
                        {
                            printf("Z\r\n");
                            critical_section_enter_blocking(&critical_section);
                            flash_range_program(flashAddress - XIP_BASE, flashData, sizeof(flashData));
                            critical_section_exit(&critical_section);
                        }

                        tmpChecksum = (rxBuf[2] << 8) + rxBuf[3];

                        printf("Rcvd: 0x%X Calc: 0x%X\r\n", tmpChecksum, fileChecksum);

                        if(true)//tmpChecksum == fileChecksum)
                        {
                            //Set magic numbers in case app fails to start
                            watchdog_hw->scratch[5] = BOOTLOADER_ENTRY_MAGIC;
                            watchdog_hw->scratch[6] = ~BOOTLOADER_ENTRY_MAGIC;

                            //Set 5 second watchdog in case application fails to start
                            watchdog_enable(5000, true);
                            watchdog_update();

                            jumpToApplication();
                        }
                        //If bad checksum, restart update process
                        else
                        {
                            printf("B");

                            //critical_section_enter_blocking(&critical_section);
                            flash_range_erase(FLASH_START_ADDRESS, FLASH_SIZE_BYTES);
                            //critical_section_exit(&critical_section);

                            fileChecksum = 0;
                            validRecordNumber = 0;
                            flashDataIndex = 0;
                            extendedAddress = 0;
                            flashAddress = 0;
                            recordState = INTEL_HEX_START_STATE;
                        }
                        break;

                    //Hex Record Data
                    case 'D':
                        for(bufIndex = 2; bufIndex < sizeof(rxBuf) && !recordComplete; bufIndex++)
                        {
                            switch(recordState)
                            {
                                case INTEL_HEX_START_STATE:
                                    if(rxBuf[bufIndex] == ':')
                                    {
                                        recordState = INTEL_HEX_BCOUNT_STATE;
                                    }
                                    break;

                                case INTEL_HEX_BCOUNT_STATE:
                                    recordLength = rxBuf[bufIndex];
                                    pHexRecord[recordIndex++] = recordLength;

                                    if(recordLength >= 1)
                                    {
                                        recordLenCntr = recordLength - 1; //0 indexed
                                    }
                                    else
                                    {
                                        recordLenCntr = 0;
                                    }

                                    recordState = INTEL_HEX_ADDR1_STATE;
                                    break;

                                case INTEL_HEX_ADDR1_STATE:
                                case INTEL_HEX_ADDR0_STATE:
                                case INTEL_HEX_TYPE_STATE:
                                    pHexRecord[recordIndex++] = rxBuf[bufIndex];

                                    if(recordLength == 0 && recordState == INTEL_HEX_TYPE_STATE)
                                    {
                                        recordState = INTEL_HEX_CSUM_STATE;
                                    }
                                    else
                                    {
                                        recordState++;
                                    }
                                    break;

                                case INTEL_HEX_DATA_STATE:
                                    pHexRecord[recordIndex++] = rxBuf[bufIndex];
                                    if(recordLenCntr-- == 0)
                                    {
                                        recordState = INTEL_HEX_CSUM_STATE;
                                    }
                                    break;

                                case INTEL_HEX_CSUM_STATE:
                                    pHexRecord[recordIndex++] = rxBuf[bufIndex];
                                    csum = 0;

                                    //Calculate checksum
                                    for(uint8_t i = 0; i < recordIndex; i++)
                                    {
                                        csum += pHexRecord[i];
                                    }

                                    if(csum == 0)
                                    {
                                        recordState = INTEL_HEX_REC1_STATE;
                                    }
                                    else
                                    {
                                        recordComplete = true;
                                        rcvdRecordNumber = 0;
                                    }
                                    break;
                                
                                case INTEL_HEX_REC1_STATE:
                                    rcvdRecordNumber = (rxBuf[bufIndex] << 8);
                                    recordState = INTEL_HEX_REC2_STATE;
                                    break;

                                case INTEL_HEX_REC2_STATE:
                                    rcvdRecordNumber += rxBuf[bufIndex];
                                    recordState = INTEL_HEX_START_STATE;
                                    recordComplete = true;
                                    break;
                            }
                        }
                        break;
                }
            }
        }

        sleep_ms(5);
    }

    return rcvdRecordNumber;
}

uint8_t Bootloader::getHexRecordLength(uint8_t* pHexRecord)
{
    // : is not in passed in record
    return hexRecord[0];
}

uint32_t Bootloader::getHexRecordAddress(uint8_t* pHexRecord)
{
    // : is not in passed in record
    return ((uint32_t)hexRecord[1] << 8) + (uint32_t)hexRecord[2];
}

uint8_t Bootloader::getHexRecordType(uint8_t* pHexRecord)
{
    // : is not in passed in record
    return hexRecord[3];
}

uint32_t Bootloader::getHexRecordExtendedAddress(uint8_t* pHexRecord)
{
    // : is not in passed in record
    return ((uint32_t)hexRecord[4] << 8) + (uint32_t)hexRecord[5];
}

void Bootloader::processHexRecord(uint8_t* pHexRecord)
{
    uint8_t recordLength, i;
    uint32_t recordAddress; 

    switch(getHexRecordType(pHexRecord))
    {
        //Data Record
        case 0:
            recordLength = getHexRecordLength(pHexRecord);
            recordAddress = (extendedAddress << 16) + getHexRecordAddress(pHexRecord);

            //Make sure start address is valid
            if(recordAddress < XIP_BASE + BOOTLOADER_SIZE_BYTES)
            {
                printf("S: 0x%x\r\n", recordAddress);
            }
            //If address isnt contiguous, write previous block and start new
            else if(recordAddress != (flashAddress + flashDataIndex))
            {
                printf("C: Addr: 0x%X Flash: 0x%X\r\n", recordAddress, flashAddress + flashDataIndex);

                if(flashDataIndex > 0)
                {
                    critical_section_enter_blocking(&critical_section);
                    flash_range_program(flashAddress - XIP_BASE, flashData, sizeof(flashData));
                    critical_section_exit(&critical_section);
                }

                flashDataIndex = 0;

                if(recordAddress % FLASH_WRITE_SIZE == 0)
                {
                    flashAddress = recordAddress;

                    printf("Flashing 0x%X\r\n", flashAddress);

                    for(i = 0; i < recordLength; i++)
                    {
                        flashData[flashDataIndex++] = pHexRecord[INTEL_HEX_RECORD_DATA_INDEX + i];
                    }
                }
                else
                {
                    printf("Q");
                }
            }
            //Else this is the start of the file or a contiguous record
            else
            {
                if(flashDataIndex + recordLength > FLASH_WRITE_SIZE)
                {
                    printf("F 0x%X\r\n", flashAddress);

                    //Write current flash data buffer
                    critical_section_enter_blocking(&critical_section);
                    flash_range_program(flashAddress - XIP_BASE, flashData, sizeof(flashData));
                    critical_section_exit(&critical_section);

                    flashDataIndex = 0;
                }

                if(flashDataIndex == 0)
                {
                    if(recordAddress % FLASH_WRITE_SIZE == 0)
                    {
                        flashAddress = recordAddress;

                        printf("Flashing 0x%X\r\n", flashAddress);
                    }
                    else
                    {
                        printf("R: 0x%X\r\n", recordAddress);
                        break;
                    }
                }

                for(i = 0; i < recordLength; i++)
                {
                    flashData[flashDataIndex++] = pHexRecord[INTEL_HEX_RECORD_DATA_INDEX + i];
                }
            }
            break;

        //End of File Record:
        case 1:
            if(flashDataIndex > 0)
            {
                printf("F 0x%X\r\n", flashAddress);

                //Write any remaining data to flash
                critical_section_enter_blocking(&critical_section);
                flash_range_program(flashAddress - XIP_BASE, flashData, sizeof(flashData));
                critical_section_exit(&critical_section);
            }
            printf("Z");
            break;

        //Extended Linear Address Record
        case 4:
            extendedAddress = getHexRecordExtendedAddress(pHexRecord);
            break;
    }

    recordLength = getHexRecordLength(pHexRecord);

    fileChecksum += ':'; //Start of Record stripped in pHexRecord
    fileChecksum += pHexRecord[0] + pHexRecord[1]; //Record Length
    fileChecksum += pHexRecord[2] + pHexRecord[3] + pHexRecord[4] + pHexRecord[5]; //Record Address
    fileChecksum += pHexRecord[6] + pHexRecord[7]; //Record Type

    for(i = 0; i < recordLength*2; i++)
    {
        fileChecksum += pHexRecord[8 + i];
    }
}
