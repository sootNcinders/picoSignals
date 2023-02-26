#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/sync.h"
#include "hardware/i2c.h"

#ifndef PCA9955_H
#define PCA9955_H

typedef enum
{
    LEDnormal = 0,
    LEDopen,
    LEDshort
}LEDerrors;

class PCA9955
{
    public:
        /// @brief Constructor for PCA9955 Driver
        /// @param i2cbus i2c bus instance
        /// @param addr i2c address - See PCA9955 Datasheet for address
        /// @param maxCurrent maximum current as set by REXT, see Datasheet
        PCA9955(i2c_inst_t *i2cbus, uint8_t addr, float maxCurrent);

        /// @brief Set the max current for the LEDs
        /// @param num LED to set
        /// @param mAmps LED current from datasheet. Capped to PCA9955 max current
        void setLEDcurrent(uint8_t num, float mAmps);

        /// @brief Set the brightnes of a LED
        /// @param num LED to set
        /// @param brightness 8bit brightness, 8-247 PWM, <8 off, >247 full on
        void setLEDbrightness(uint8_t num, uint8_t brightness);

        /// @brief Checks for faults on all active LEDs
        /// @return true if error is found
        bool checkErrors();

        /// @brief Prints all PCA9955 Registers for debugging
        void printRegisters();

        /// @brief Gets the error mode of the specified LED.
        /// @param num LED to check
        /// @return LED status using LEDerrors ENUM
        uint8_t getError(uint8_t num);

        /// @brief Puts the PCA9955 in low power mode - Turns off all LEDs
        void sleep();

        /// @brief Takes the PCA9955 out of low power mode - LED states will need to be need to be reset
        void wake();

        /// @brief Set the critical section used to protect the i2c bus
        /// @param cs Critical section pointer
        void setCriticalSection(critical_section_t* cs);

    private:
        i2c_inst_t* _bus;
        uint8_t _address;
        critical_section_t* critSec;
        float _maxCurrent;
        uint8_t ledState[4];
        uint8_t _errors[8];

        //
        void clearFaults();

};

#endif 
