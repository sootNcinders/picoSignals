#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pca9955.h"
#include "head.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#ifndef GARHEAD_H
#define GARHEAD_H

class GARHEAD: public Head
{
    public:
        /// @brief Constructor for a Green-Amber-Red Signal Head
        /// @param driver PCA9955 driver instance
        /// @param gPin Green pin on PCA9955
        /// @param aPin Amber pin on PCA9955
        /// @param rPin Red pin on PCA9955
        GARHEAD(PCA9955 *driver, SemaphoreHandle_t mutex, uint8_t gPin, uint8_t aPin, uint8_t rPin, uint8_t lPin);

        /// @brief Initializes the head and sets the current for each LED
        /// @param gCurrent Green LED current
        /// @param aCurrent Amber LED current
        /// @param rCurrent Red LED current
        void init(float gCurrent, float aCurrent, float rCurrent, float lCurrent);

        /// @brief Sets the head to the specified color
        /// @param color Color from headState ENUM
        /// @return true if no errors
        bool setHead(uint8_t color);

        bool setHeadFromISR(uint8_t color);

        /// @brief Gets the color the head is currently set to
        /// @return The current color using headState ENUM
        uint8_t getColor();

        /// @brief Gets the current LED errors
        /// @return LED error from PCA9955 driver
        uint8_t getError();

        /// @brief Sets the max brightness of the specified color
        /// @param color Color to set
        /// @param brightness 8 bit brightness level 0-255
        void setColorBrightness(uint8_t color, uint8_t brightness);

        /// @brief Sets the brightness level for the whole head
        /// @param brightness 8 bit brightness level 0-255
        void setHeadBrightness(float brightness);

        void setHeadBrightnessFromISR(float brightness);

    private:
        /// @brief Fades from one color to the next
        /// @param oldColor current color
        /// @param newColor next color
        void fade(uint8_t oldColor, uint8_t newColor);

        BaseType_t fadeFromISR(uint8_t oldColor, uint8_t newColor);

        PCA9955 *_driver;

        uint8_t _color;

        uint8_t _gPin;
        uint8_t _aPin;
        uint8_t _rPin;
        uint8_t _lPin;

        uint8_t _brightness[off];

        float _headBrightness;

        SemaphoreHandle_t _mutex;

};

#endif