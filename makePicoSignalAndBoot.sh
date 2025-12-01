#!/bin/bash

INPUT_ELF1="Bootloader/build/bootloader.elf"
INPUT_ELF2="build/picoSignals.elf"
INPUT_HEX1="Bootloader/build/bootloader.hex"
INPUT_HEX2="build/picoSignals.hex"
OUTPUT_UF2="Builds/V4R0/PicoSignalsAndBoot-V4R0.uf2"
OUTPUT_HEX="Builds/V4R0/PicoSignals-V4R0.hex"
OUTPUT_HEX_COMBINED="Builds/V4R0/PicoSignalsAndBoot-V4R0.hex"
OUTPUT_BIN="Builds/V4R0/PicoSignalsAndBoot-V4R0.bin"

# Temporary files for combined ELF
COMBINED_ELF="PicoSignalsAndBootV4R0.elf"

#Copy app hex to output location
srec_cat $INPUT_HEX2 -intel -o $OUTPUT_HEX -intel -line_length=44

srec_cat $INPUT_HEX1 -intel -generate 0x1000B6B8 0x1000C000 -constant 0 $INPUT_HEX2 -intel -o $OUTPUT_HEX_COMBINED -intel -line_length=44

#arm-none-eabi-objcopy -O elf32-littlearm -I ihex $OUTPUT_HEX_COMBINED $COMBINED_ELF

python3 ../../uf2/uf2#/utils/uf2conv.py -c -f RP2040 $OUTPUT_HEX_COMBINED -o $OUTPUT_UF2
python3 ../../uf2/uf2#/utils/uf2conv.py --info $OUTPUT_UF2

# Check if the conversion was successful
if [ $? -ne 0 ]; then
    echo "Error converting to UF2."
    exit 1
fi

# Clean up temporary files
#rm $COMBINED_ELF

echo "Successfully combined ELF files into $OUTPUT_UF2."