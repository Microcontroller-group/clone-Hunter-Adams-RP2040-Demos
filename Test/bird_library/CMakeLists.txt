# Bird Library CMakeLists.txt
cmake_minimum_required(VERSION 3.13)

# include the sdk.cmake file
include(pico_sdk_import.cmake)

# give the project a name
project(bird_library C CXX ASM)

# initialize the sdk
pico_sdk_init()

# Create the library
add_library(bird_lib STATIC
    bird_lib.c
)

# Include directories for the library
target_include_directories(bird_lib PUBLIC .)

# Link required libraries
target_link_libraries(bird_lib pico_stdlib hardware_spi hardware_sync)

# Optional: Create the original beep_beep executable as well for testing
add_executable(original_beep_beep beep_beep.c)
target_link_libraries(original_beep_beep pico_stdlib pico_bootsel_via_double_reset hardware_spi hardware_sync)
pico_add_extra_outputs(original_beep_beep)
