#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pca9955.h"
#include "head.h"

#ifndef RGBHEAD_H
#define RGBHEAD_H

class RGBHEAD: public Head
{
    public:
        /// @brief Constructor for a Green-Amber-Red Signal Head
        /// @param driver PCA9955 driver instance
        /// @param gPin Green pin on PCA9955
        /// @param aPin Amber pin on PCA9955
        /// @param rPin Red pin on PCA9955
        RGBHEAD(PCA9955 *driver, uint8_t rPin, uint8_t gPin, uint8_t bPin);

        /// @brief Initializes the head and sets the current for each LED
        /// @param rCurrent Red LED Current
        /// @param gCurrent Green LED Current
        /// @param bCurrent Blue LED Current
        void init(float rCurrent, float gCurrent, float bCurrent);

        /// @brief Sets the head to the specified color
        /// @param color Color from headState ENUM
        /// @return true if no errors
        bool setHead(uint8_t color);

        /// @brief Gets the color the head is currently set to
        /// @return The current color using headState ENUM
        uint8_t getColor();

        /// @brief Gets the current LED errors
        /// @return bit packed LED errors from PCA9955 driver, 2 bits each xRGB
        uint8_t getError();

        /// @brief Set the RGB levels for each color
        /// @param color Color to set
        /// @param rLevel Red Level
        /// @param gLevel Green Level
        /// @param bLevel Blue Level
        void setColorLevels(uint8_t color, uint8_t rLevel, uint8_t gLevel, uint8_t bLevel);

        /// @brief Sets the brightness level for the whole head
        /// @param brightness 8 bit brightness level 0-255
        void setHeadBrightness(float brightness);

    private:
        PCA9955 *_driver;

        uint8_t _color;

        uint8_t _rPin;
        uint8_t _gPin;
        uint8_t _bPin;

        uint8_t _levels[off][3];

        float _headBrightness;

        typedef enum
        {
            r = 0,
            g = 1,
            b = 2
        }RGB;

};

#endif