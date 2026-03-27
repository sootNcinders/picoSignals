#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"

#ifndef INPUT_H
#define INPUT_H

#define PCA9674_ID 0x00580200
#define PCA9554_ID 0x00000000

enum inputChip
{
    unknownchip = 0,
    pca9674chip,
    pca9554chip
};

class Input
{
    public:
        /// @brief Set which pins will be used for inputs and which will be ignored
        /// @param mask Single byte, each bit representing a pin
        virtual bool inputMask(uint8_t mask){return false;};

        /// @brief Returns the input state of a specified pin
        /// @param num Pin to check
        /// @param update Refresh the pin state in memory
        /// @return Pin state Active LOW
        virtual bool getInput(uint8_t num, bool update){return false;};

        /// @brief loads current state of all pins to memory
        virtual void updateInputs(void){};

        inputChip getInputChip(i2c_inst_t *i2cbus, uint8_t addr);
};

#endif
