# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# cmake version
cmake_minimum_required(VERSION 3.13)

# include the sdk.cmake file
include(pico_sdk_import.cmake)

# give the project a name (anything you want)
project(Audio_DMA_Demo C CXX ASM)

# initialize the sdk
pico_sdk_init()

add_executable(Audio_DMA_Demo)

target_sources(Audio_DMA_Demo PRIVATE dma-demo.c)

target_link_libraries(Audio_DMA_Demo pico_stdlib pico_bootsel_via_double_reset hardware_dma hardware_spi)

# create map/bin/hex file etc.
pico_add_extra_outputs(Audio_DMA_Demo)

