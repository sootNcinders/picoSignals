cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(test_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_subdirectory(./FatFs_SPI build)
add_subdirectory(./ArduinoJson)

add_executable(picoSignals
                picoSignals.cpp
                pca9674.cpp
                pca9955.cpp
                RFM95/RFM95.cpp
                GARhead.cpp
                RGBhead.cpp
                hw_config.c
                utils.c
        )

# pull in common dependencies and additional i2c hardware support
target_link_libraries(picoSignals 
                        pico_stdlib 
                        pico_multicore
                        pico_sync
                        hardware_i2c 
                        hardware_spi
                        hardware_adc
                        FatFs_SPI
                        ArduinoJson
                        )

# enable usb output, disable uart output
pico_enable_stdio_usb(picoSignals 1)
pico_enable_stdio_uart(picoSignals 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(picoSignals)
