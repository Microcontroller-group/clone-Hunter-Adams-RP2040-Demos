/**
 * Bird Library Usage Demo
 * Demonstrates how to use the bird audio library for generating bird-like chirps and swoops
 * 
 * Hardware connections:
 * GPIO 5 (pin 7) Chip select for DAC
 * GPIO 6 (pin 9) SCK/spi0_sclk for DAC
 * GPIO 7 (pin 10) MOSI/spi0_tx for DAC
 * GPIO 2 (pin 4) GPIO output for timing ISR (optional, for debugging)
 * GPIO 25 (pin 40) LED for visual feedback
 * 3.3v (pin 36) -> VCC on DAC 
 * GND (pin 3)  -> GND on DAC 
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "pt_cornell_rp2040_v1_4.h"
#include "bird_lib.h"

// Button pins for interactive control (optional)
#define BUTTON_CHIRP_PIN 15  // Button to trigger chirp
#define BUTTON_SWOOP_PIN 14  // Button to trigger swoop
#define BUTTON_MODE_PIN  13  // Button to toggle manual/auto mode

// This thread runs on core 0 - manages bird audio and user interaction
static PT_THREAD (protothread_core_0(struct pt *pt))
{
    // Indicate thread beginning
    PT_BEGIN(pt) ;
    
    static bool chirp_button_pressed = false;
    static bool swoop_button_pressed = false;
    static bool mode_button_pressed = false;
    static bool last_chirp_button_state = false;
    static bool last_swoop_button_state = false;
    static bool last_mode_button_state = false;
    
    while(1) {
        // Read button states (if buttons are connected)
        bool current_chirp_state = !gpio_get(BUTTON_CHIRP_PIN); // Active low
        bool current_swoop_state = !gpio_get(BUTTON_SWOOP_PIN); // Active low
        bool current_mode_state = !gpio_get(BUTTON_MODE_PIN);   // Active low
        
        // Detect button presses (rising edges)
        if (current_chirp_state && !last_chirp_button_state) {
            chirp_button_pressed = true;
        }
        if (current_swoop_state && !last_swoop_button_state) {
            swoop_button_pressed = true;
        }
        if (current_mode_state && !last_mode_button_state) {
            mode_button_pressed = true;
        }
        
        last_chirp_button_state = current_chirp_state;
        last_swoop_button_state = current_swoop_state;
        last_mode_button_state = current_mode_state;
        
        // Handle mode button press - toggle manual/auto mode
        if (mode_button_pressed) {
            mode_button_pressed = false;
            bool current_manual = bird_is_manual_mode();
            bird_set_manual_mode(!current_manual);
            printf("Switched to %s mode\n", !current_manual ? "MANUAL" : "AUTOMATIC");
        }
        
        // Handle chirp button press
        if (chirp_button_pressed) {
            chirp_button_pressed = false;
            printf("Triggering CHIRP sound...\n");
            bird_trigger_chirp();
        }
        
        // Handle swoop button press  
        if (swoop_button_pressed) {
            swoop_button_pressed = false;
            printf("Triggering SWOOP sound...\n");
            bird_trigger_swoop();
        }
        
        // Update bird LED (non-blocking)
        bird_led_update();
        
        // Print bird state periodically
        static int print_counter = 0;
        print_counter++;
        if (print_counter >= 1000) {  // Print every ~1 second (assuming 1ms yield)
            print_counter = 0;
            int state = bird_get_state();
            bool manual = bird_is_manual_mode();
            
            printf("Mode: %s | State: ", manual ? "MANUAL" : "AUTO");
            switch (state) {
                case BIRD_IDLE:
                    printf("IDLE\n");
                    break;
                case BIRD_CHIRP:
                    printf("CHIRP\n");
                    break;
                case BIRD_SWOOP:
                    printf("SWOOP\n");
                    break;
                default:
                    printf("UNKNOWN (%d)\n", state);
                    break;
            }
        }

        // Yield for 1 ms
        PT_YIELD_usec(1000) ;
    }
    // Indicate thread end
    PT_END(pt) ;
}

int main() {
    // Initialize stdio/uart (printf won't work unless you do this!)
    stdio_init_all();
    printf("Bird Audio Library Demo Starting!\n");
    
    // Initialize button pins (optional)
    gpio_init(BUTTON_CHIRP_PIN);
    gpio_set_dir(BUTTON_CHIRP_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_CHIRP_PIN);  // Enable pull-up resistor
    
    gpio_init(BUTTON_SWOOP_PIN);
    gpio_set_dir(BUTTON_SWOOP_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_SWOOP_PIN);  // Enable pull-up resistor
    
    gpio_init(BUTTON_MODE_PIN);
    gpio_set_dir(BUTTON_MODE_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_MODE_PIN);  // Enable pull-up resistor
    
    // Initialize the bird audio library
    printf("Initializing bird audio library...\n");
    bird_init();
    
    // Optional: Configure custom timing (attack, decay, sustain, repeat interval in ms)
    // bird_set_timing(5, 5, 120, 1250);  // Faster bird calls
    printf("Bird audio library initialized!\n");
    
    // Start bird audio automatically in automatic mode
    printf("Starting bird audio generation in AUTOMATIC mode...\n");
    printf("Button controls:\n");
    printf("  GPIO %d: Trigger CHIRP sound\n", BUTTON_CHIRP_PIN);
    printf("  GPIO %d: Trigger SWOOP sound\n", BUTTON_SWOOP_PIN);
    printf("  GPIO %d: Toggle MANUAL/AUTO mode\n", BUTTON_MODE_PIN);
    bird_start();
    
    // Add core 0 threads
    pt_add_thread(protothread_core_0);
    
    printf("Starting protothread scheduler...\n");
    
    // Start scheduling core 0 threads
    pt_schedule_start ;
    
    // Should never reach here
    return 0;
}
