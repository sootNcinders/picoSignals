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

include(../../../pico-sdk/external/pico_sdk_import.cmake)

set(PICO_BOARD pico CACHE STRING "Board type")
set(PICO_PLATFORM rp2040)
set(PICOTOOL_FETCH_FROM_GIT_PATH ./picotool)

# Pull in Pico SDK
include(pico_sdk_import.cmake)

project(bootloader C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()


add_executable(bootloader
                bootloader.cpp
                ../RFM95/RFM95.cpp
        )

pico_set_linker_script(bootloader ${CMAKE_SOURCE_DIR}/bootloader.ld)

target_include_directories(bootloader PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# pull in common dependencies and additional i2c hardware support
target_link_libraries(bootloader 
                        cmsis_core
                        hardware_dma
                        pico_stdlib 
                        pico_sync
                        pico_flash
                        hardware_irq
                        hardware_spi
                        hardware_flash
                        hardware_exception
                        hardware_clocks
                        hardware_resets
	                    hardware_structs
                        )

# enable usb output, disable uart output
pico_enable_stdio_usb(bootloader 1)
pico_enable_stdio_uart(bootloader 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(bootloader)

string(APPEND CMAKE_EXE_LINKER_FLAGS "-Wl,--print-memory-usage")
