#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/sync.h"
#include "hardware/i2c.h"

#ifndef PCA9674_H
#define PCA9674_h

#define PCA9674_INT_MODE GPIO_IRQ_EDGE_FALL

class pca9674
{
    public:
        /// @brief Constructor for the PCA9674 instance
        /// @param i2cbus Pico i2c bus instance- i2c0, i2c1, or i2c_default
        /// @param addr Address as defined by the address pins on the PCA9674 chip, see datasheet for values
        pca9674(i2c_inst_t *i2cbus, uint8_t addr);

        /// @brief Set which pins will be used for inputs and which will be ignored
        /// @param mask Single byte, each bit representing a pin
        void inputMask(uint8_t mask);

        /// @brief Returns the input state of a specified pin
        /// @param num Pin to check
        /// @param update Refresh the pin state in memory
        /// @return Pin state Active LOW
        bool getInput(uint8_t num, bool update);

        /// @brief loads current state of all pins to memory
        void updateInputs();

        /// @brief Set the critical section used to protect the i2c bus
        /// @param cs Critical section pointer
        void setCriticalSection(critical_section_t* cs);

    private:
        uint8_t address;
        uint8_t buffer;
        i2c_inst_t* bus; 
        critical_section_t* critSec;

        uint8_t lastBuffer;

        bool pinState[8];
};

#endif
