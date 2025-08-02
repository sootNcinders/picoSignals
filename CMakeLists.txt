# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)
include(FetchContent)

include(../../pico-sdk/external/pico_sdk_import.cmake)

set(PICO_BOARD pico CACHE STRING "Board type")
set(PICO_PLATFORM rp2040)
set(PICOTOOL_FETCH_FROM_GIT_PATH ./picotool)
# Pull in Pico SDK
include(pico_sdk_import.cmake)
# Pull in FreeRTOS
include(FreeRTOS_Kernel_import.cmake)
project(picoSignals C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_subdirectory(./FatFs_SPI build)
add_subdirectory(./ArduinoJson)

add_executable(picoSignals
                main.cpp
                LED.cpp
                battery.cpp
                radio.cpp
                io.cpp
                ctc.cpp
                heads.cpp
                menu.cpp
                overlay.cpp
                pca9674.cpp
                pca9955.cpp
                RFM95/RFM95.cpp
                GARhead.cpp
                RGBhead.cpp
                hw_config.c
        )

pico_set_linker_script(picoSignals ${CMAKE_SOURCE_DIR}/picoSignals.ld)

target_include_directories(picoSignals PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_include_directories(picoSignals PRIVATE ./Headers)

# pull in common dependencies and additional i2c hardware support
target_link_libraries(picoSignals 
                        pico_stdlib 
                        pico_sync
                        pico_flash
                        hardware_i2c 
                        hardware_irq
                        hardware_spi
                        hardware_adc
                        hardware_timer
                        hardware_watchdog
                        hardware_flash
                        hardware_exception
                        hardware_clocks
                        FreeRTOS-Kernel
                        FreeRTOS-Kernel-Heap4
                        FatFs_SPI
                        ArduinoJson
                        )

# enable usb output, disable uart output
pico_enable_stdio_usb(picoSignals 1)
pico_enable_stdio_uart(picoSignals 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(picoSignals)

string(APPEND CMAKE_EXE_LINKER_FLAGS "-Wl,--print-memory-usage")
