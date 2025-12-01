#include <stdio.h>
#include <stdlib.h>

#include "RP2040.h"
#include "pico/stdlib.h"
#include "pico/critical_section.h"
#include "hardware/flash.h"
#include "hardware/structs/watchdog.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"

#include "../RFM95/RFM95.h"

#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#define FLASH_WRITE_SIZE 256
#define FLASH_START_ADDRESS 0x10000000

#define INTEL_HEX_RECORD_DATA_INDEX 4

#define MSGBUFSIZE 8

#define BOOTLOADER_ENTRY_MAGIC 0xb105f00d

#define BOOTLOADER_SIZE_BYTES 0xC000 //48KB
#define FLASH_SIZE_BYTES ((2048 * 1024) - (BOOTLOADER_SIZE_BYTES)) //Reserve 32KB for bootloader

#define GOODLED  6
#define ERRORLED 7
#define RXLED    8
#define TXLED    9

#define RADIOINT 15

// HexRecord states
enum {
	INTEL_HEX_START_STATE = 0,
	INTEL_HEX_BCOUNT_STATE,
	INTEL_HEX_ADDR1_STATE,
	INTEL_HEX_ADDR0_STATE,
	INTEL_HEX_TYPE_STATE,
	INTEL_HEX_DATA_STATE,
	INTEL_HEX_CSUM_STATE,
	INTEL_HEX_REC1_STATE,
	INTEL_HEX_REC2_STATE,
	INTEL_HEX_ERR_STATE
};

// Address of binary information header
uint8_t *flash_target_contents = (uint8_t *) (XIP_BASE + BOOTLOADER_SIZE_BYTES + 0xD4);

class Bootloader
{
    public:
        static void doUpdate(void);
        static bool checkForUpdate(void);
        static void jumpToApplication(void);

        static RFM95 radio;

    private:
        static uint16_t waitForHexRecord(uint8_t* pHexRecord);
        static uint8_t getHexRecordLength(uint8_t* pHexRecord);
        static uint32_t getHexRecordAddress(uint8_t* pHexRecord);
        static uint8_t getHexRecordType(uint8_t* pHexRecord);
        static uint32_t getHexRecordExtendedAddress(uint8_t* pHexRecord);
        static void processHexRecord(uint8_t* pHexRecord);

        static uint8_t hexRecord[0x30];
        static uint8_t flashData[FLASH_WRITE_SIZE];

        static uint16_t fileChecksum;
        static uint16_t validRecordNumber;
        static uint16_t flashDataIndex;

        static uint32_t extendedAddress;
        static uint32_t flashAddress;

        static critical_section_t critical_section;
};
#endif
