#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pca9955.h"

#ifndef HEAD_H
#define HEAD_H

typedef enum
{
    green = 0,
    amber,
    red,
    lunar,
    off
}headState;

class Head
{
    public:
        /// @brief Sets the head to the specified color
        /// @param color Color from headState ENUM
        /// @return true if no errors
        virtual bool setHead(uint8_t color){return true;}

        /// @brief Gets the color the head is currently set to
        /// @return The current color using headState ENUM
        virtual uint8_t getColor(){return 0;}

        /// @brief Gets the current LED errors
        /// @return LED error from PCA9955 driver
        virtual uint8_t getError(){return 0;}

        /// @brief Sets the brightness level for the whole head
        /// @param brightness 8 bit brightness level 0-255
        virtual void setHeadBrightness(float brightness){}
};
#endif