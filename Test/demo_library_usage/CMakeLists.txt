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

# give the project a name
project(demo_library_usage C CXX ASM)

# initialize the sdk
pico_sdk_init()

# Add the blinky library subdirectory
add_subdirectory(../blinky_library blinky_library)

# Create the main executable
add_executable(demo_library_usage main.c)

# Link libraries
target_link_libraries(demo_library_usage 
    pico_stdlib 
    pico_bootsel_via_double_reset
    blinky_lib
)

# create map/bin/hex file etc.
pico_add_extra_outputs(demo_library_usage)
