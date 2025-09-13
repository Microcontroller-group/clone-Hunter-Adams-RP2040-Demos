/**
 * @file main.c
 * @brief Bird Keypad Demo - Integrated bird sounds with keypad input
 * 
 * This application combines the bird_lib and keypad_lib to create an interactive
 * bird sound generator. Pressing key '1' produces a swoop sound, and pressing
 * key '2' produces a chirp sound.
 * 
 * HARDWARE CONNECTIONS:
 * 
 * KEYPAD:
 *  - GPIO 9   -->  330 ohms  --> Pin 1 (button row 1)
 *  - GPIO 10  -->  330 ohms  --> Pin 2 (button row 2)
 *  - GPIO 11  -->  330 ohms  --> Pin 3 (button row 3)
 *  - GPIO 12  -->  330 ohms  --> Pin 4 (button row 4)
 *  - GPIO 13  -->     Pin 5 (button col 1)
 *  - GPIO 14  -->     Pin 6 (button col 2)
 *  - GPIO 15  -->     Pin 7 (button col 3)
 * 
 * AUDIO (DAC):
 *  - GPIO 5 (pin 7) Chip select
 *  - GPIO 6 (pin 9) SCK/spi0_sclk
 *  - GPIO 7 (pin 10) MOSI/spi0_tx
 *  - GPIO 2 (pin 4) GPIO output for timing ISR
 *  - 3.3v (pin 36) -> VCC on DAC 
 *  - GND (pin 3)  -> GND on DAC 
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
 * 
 * LED:
 *  - GPIO 25 ---> LED (built-in LED)
 */

#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/sync.h"
#include "hardware/spi.h"
#include "hardware/clocks.h"

// VGA graphics library
#include "pt_cornell_rp2040_v1_4.h"

// Bird library modules
#include "dds.h"
#include "dac.h"
#include "audio_envelope.h"
#include "bird_generator.h"

// Keypad library modules
#include "keypad.h"
#include "display.h"
#include "gpio_config.h"
#include "keypad_fsm.h"

// LED pin
#define LED 25

// Keypad key mappings
#define KEY_1_INDEX 1  // Key '1' corresponds to index 0
#define KEY_2_INDEX 2  // Key '2' corresponds to index 1

// This thread runs on core 0
static PT_THREAD (protothread_core_0(struct pt *pt))
{
    // Indicate thread beginning
    PT_BEGIN(pt);

    // FSM context for keypad debouncing
    static keypad_fsm_t fsm;
    static int fsm_initialized = 0;
    
    // Variables for keypad scanning and bird sound control
    static int current_key;
    static int prev_key = -1;
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
        
        // Check for key press events and trigger bird sounds
        if (state_changed && keypad_fsm_is_key_pressed(&fsm)) {
            int pressed_key = keypad_fsm_get_current_key(&fsm);        

            if (pressed_key == KEY_1_INDEX) {
                printf("Key '1' pressed! Playing swoop sound...\n");
                bird_trigger_swoop();
            } else if (pressed_key == KEY_2_INDEX) {
                printf("Key '2' pressed! Playing chirp sound...\n");
                bird_trigger_chirp();
            } else {
                printf("Key %d pressed (not mapped to bird sounds)\n", pressed_key);
            }
        }

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

        // Yield for 30ms (same as keypad demo)
        PT_YIELD_usec(30000);
    }
    // Indicate thread end
    PT_END(pt);
}

int main() {
    // Overclock for better performance
    set_sys_clock_khz(150000, true);

    // Initialize stdio
    stdio_init_all();
    printf("🐦 Bird Keypad Demo Started! 🐦\n");
    printf("Press key '1' for swoop sound, key '2' for chirp sound\n");

    // Initialize bird sound modules
    dac_init();
    dds_init();
    audio_envelope_init();
    bird_generator_init();

    // Initialize display system
    display_init();
    display_draw_layout();

    // Initialize GPIO systems
    gpio_config_led_init();
    keypad_init();

    // Map LED to GPIO port, make it low
    gpio_init(LED);
    gpio_set_dir(LED, GPIO_OUT);
    gpio_put(LED, 0);

    // Add core 0 threads
    pt_add_thread(protothread_core_0);

    // Start scheduling core 0 threads
    pt_schedule_start;
}
