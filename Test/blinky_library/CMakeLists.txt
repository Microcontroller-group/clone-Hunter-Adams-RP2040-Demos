# Blinky Library CMakeLists.txt
cmake_minimum_required(VERSION 3.13)

# include the sdk.cmake file
include(pico_sdk_import.cmake)

# give the project a name
project(blinky_library C CXX ASM)

# initialize the sdk
pico_sdk_init()

# Create the library
add_library(blinky_lib STATIC
    blinky_lib.c
)

# Include directories for the library
target_include_directories(blinky_lib PUBLIC .)

# Link required libraries
target_link_libraries(blinky_lib pico_stdlib)

# Optional: Create the original blinky executable as well for testing
add_executable(original_blinky blinky.c)
target_link_libraries(original_blinky pico_stdlib pico_bootsel_via_double_reset)
pico_add_extra_outputs(original_blinky)