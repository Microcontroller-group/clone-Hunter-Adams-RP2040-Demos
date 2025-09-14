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

cmake_minimum_required(VERSION 3.13)

# include the sdk.cmake file
include(pico_sdk_import.cmake)

# set the project name
project(bird_keypad_demo C CXX ASM)

# initialize the sdk
pico_sdk_init()

# Add executable. Default name is the same as the project name.
add_executable(bird_keypad_demo main.c sound_recorder.c)

# Add the bird_lib source files
target_sources(bird_keypad_demo PRIVATE
    ../bird_lib/bird_generator.c
    ../bird_lib/dds.c
    ../bird_lib/dac.c
    ../bird_lib/audio_envelope.c
)

# Add the keypad_lib source files
target_sources(bird_keypad_demo PRIVATE
    ../keypad_lib/keypad.c
    ../keypad_lib/keypad_fsm.c
    ../keypad_lib/display.c
    ../keypad_lib/gpio_config.c
)

# Add the VGA graphics source files
target_sources(bird_keypad_demo PRIVATE
    ../keypad_lib/vga16_graphics_v2.c
    ../keypad_lib/glcdfont.c
)

# Add the PIO source files - these will be processed by pico_generate_pio_header
pico_generate_pio_header(bird_keypad_demo ${CMAKE_CURRENT_LIST_DIR}/../keypad_lib/hsync.pio)
pico_generate_pio_header(bird_keypad_demo ${CMAKE_CURRENT_LIST_DIR}/../keypad_lib/vsync.pio)
pico_generate_pio_header(bird_keypad_demo ${CMAKE_CURRENT_LIST_DIR}/../keypad_lib/rgb.pio)

# Add include directories
target_include_directories(bird_keypad_demo PRIVATE
    ../bird_lib
    ../keypad_lib
)

# Add the protothreads library
target_sources(bird_keypad_demo PRIVATE
    ../bird_lib/pt_cornell_rp2040_v1_4.h
)

# Link the pico_stdlib library
target_link_libraries(bird_keypad_demo pico_stdlib hardware_pio hardware_dma hardware_spi hardware_timer hardware_irq hardware_sync hardware_clocks)

# enable usb output, initialize stdio
pico_enable_stdio_usb(bird_keypad_demo 1)
pico_enable_stdio_uart(bird_keypad_demo 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(bird_keypad_demo)
