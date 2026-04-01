#!/bin/bash

VERSION="V4R0"

INPUT_ELF1="Bootloader/build/bootloader.elf"
INPUT_ELF2="build/picoSignals.elf"
INPUT_HEX1="Bootloader/build/bootloader.hex"
INPUT_HEX2="build/picoSignals.hex"
OUTPUT_FOLDER="Builds/$VERSION"
OUTPUT_UF2="Builds/$VERSION/PicoSignalsAndBoot-$VERSION.uf2"
OUTPUT_HEX="Builds/$VERSION/PicoSignals-$VERSION.hex"
OUTPUT_HEX_COMBINED="Builds/$VERSION/PicoSignalsAndBoot-$VERSION.hex"
OUTPUT_BIN="Builds/$VERSION/PicoSignalsAndBoot-$VERSION.bin"

# Temporary files for combined ELF
COMBINED_ELF="PicoSignalsAndBoot$VERSION.elf"

# Create output folder if it doesn't exist
mkdir -p $OUTPUT_FOLDER

#Copy app hex to output location
srec_cat $INPUT_HEX2 -intel -o $OUTPUT_HEX -intel -line_length=44

srec_cat $INPUT_HEX1 -intel -generate 0x1000B798 0x1000C000 -constant 0 $INPUT_HEX2 -intel -o $OUTPUT_HEX_COMBINED -intel -line_length=44


python3 ../../uf2/uf2#/utils/uf2conv.py -c -f RP2040 $OUTPUT_HEX_COMBINED -o $OUTPUT_UF2
python3 ../../uf2/uf2#/utils/uf2conv.py --info $OUTPUT_UF2

# Check if the conversion was successful
if [ $? -ne 0 ]; then
    echo "Error converting to UF2."
    exit 1
fi

echo "Successfully combined ELF files into $OUTPUT_UF2."