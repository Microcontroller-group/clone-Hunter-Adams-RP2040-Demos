/**
 * @file main.c
 * @brief Main application for keypad demo
 * 
 * This is the main application file that orchestrates the keypad demo.
 * It initializes all systems and starts the protothread scheduler.
 * 
 * HARDWARE CONNECTIONS:
 * KEYPAD:
 *  - GPIO 9   -->  330 ohms  --> Pin 1 (button row 1)
 *  - GPIO 10  -->  330 ohms  --> Pin 2 (button row 2)
 *  - GPIO 11  -->  330 ohms  --> Pin 3 (button row 3)
 *  - GPIO 12  -->  330 ohms  --> Pin 4 (button row 4)
 *  - GPIO 13  -->     Pin 5 (button col 1)
 *  - GPIO 14  -->     Pin 6 (button col 2)
 *  - GPIO 15  -->     Pin 7 (button col 3)
 * 
 * VGA:
 *  - GPIO 16 ---> VGA Hsync
 *  - GPIO 17 ---> VGA Vsync
 *  - GPIO 18 ---> 470 ohm resistor ---> VGA Green 
 *  - GPIO 19 ---> 330 ohm resistor ---> VGA Green
 *  - GPIO 20 ---> 330 ohm resistor ---> VGA Blue
 *  - GPIO 21 ---> 330 ohm resistor ---> VGA Red
 *  - RP2040 GND ---> VGA GND
 * 
 * SERIAL:
 *  - GPIO 0        -->     UART RX (white)
 *  - GPIO 1        -->     UART TX (green)
 *  - RP2040 GND    -->     UART GND
 */

#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/sync.h"
#include "hardware/spi.h"
#include "hardware/clocks.h"

// VGA graphics library
#include "pt_cornell_rp2040_v1_4.h"

// Application modules
#include "keypad.h"
#include "display.h"
#include "gpio_config.h"
#include "keypad_fsm.h"

// This thread runs on core 0
static PT_THREAD (protothread_core_0(struct pt *pt))
{
    // Indicate thread beginning
    PT_BEGIN(pt) ;

    // FSM context
    static keypad_fsm_t fsm;
    static int fsm_initialized = 0;
    
    // Some variables
    static int current_key;
    static int state_changed;

    while(1) {
        // Initialize FSM on first run
        if (!fsm_initialized) {
            keypad_fsm_init(&fsm, 3); // 3 consecutive readings for debouncing
            fsm_initialized = 1;
        }

        // Toggle LED to show activity
        gpio_config_led_toggle();

        // Scan the keypad
        current_key = keypad_scan();

        // Update FSM with new keypad input
        state_changed = keypad_fsm_update(&fsm, current_key);

        // Update display with FSM information
        display_update_key_with_fsm(
            keypad_fsm_get_current_key(&fsm),
            keypad_fsm_get_state(&fsm),
            keypad_fsm_is_key_pressed(&fsm)
        );

        // Print FSM state to terminal for debugging
        printf("\nKey: %d, State: %s, Pressed: %s", 
               keypad_fsm_get_current_key(&fsm),
               keypad_fsm_state_to_string(keypad_fsm_get_state(&fsm)),
               keypad_fsm_is_key_pressed(&fsm) ? "YES" : "NO");

        PT_YIELD_usec(30000) ;
    }
    // Indicate thread end
    PT_END(pt) ;
}

int main() {
    // Overclock
    set_sys_clock_khz(150000, true) ;

    // Initialize stdio
    stdio_init_all();

    // Initialize display system
    display_init();
    display_draw_layout();

    // Initialize GPIO systems
    gpio_config_led_init();
    keypad_init();

    // Add core 0 threads
    pt_add_thread(protothread_core_0) ;

    // Start scheduling core 0 threads
    pt_schedule_start ;
}
