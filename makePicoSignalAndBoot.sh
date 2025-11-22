#!/bin/bash

INPUT_ELF1="Bootloader/build/bootloader.elf"
INPUT_ELF2="build/picoSignals.elf"
INPUT_HEX1="build/picoSignals.hex"
OUTPUT_UF2="Builds/V4R0/PicoSignalsAndBoot-V4R0.uf2"
OUTPUT_HEX="Builds/V4R0/PicoSignals-V4R0.hex"
OUTPUT_HEX_COMBINED="Builds/V4R0/PicoSignalsAndBoot-V4R0.hex"

# Temporary files for combined ELF
COMBINED_ELF="PicoSignalsAndBootV4R0.elf"

# Combine the ELF files using the linker
#arm-none-eabi-objcopy --update-section .boot=$INPUT_ELF1 $INPUT_ELF2 $COMBINED_ELF
arm-none-eabi-objcopy --update-section .app_bin=$INPUT_ELF2 $INPUT_ELF1 $COMBINED_ELF
arm-none-eabi-objcopy -O ihex $COMBINED_ELF $OUTPUT_HEX_COMBINED

# Check if the combination was successful
if [ $? -ne 0 ]; then
    echo "Error combining ELF files."
    exit 1
fi

# Convert the combined ELF to UF2 format
elf2uf2-rs -v $COMBINED_ELF $OUTPUT_UF2

# Check if the conversion was successful
if [ $? -ne 0 ]; then
    echo "Error converting to UF2."
    exit 1
fi

# Clean up temporary files
#rm $COMBINED_ELF

echo "Successfully combined ELF files into $OUTPUT_UF2."