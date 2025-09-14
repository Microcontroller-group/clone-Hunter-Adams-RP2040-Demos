/**
 *  V. Hunter Adams (vha3@cornell.edu)
 
    A timer interrupt on core 0 generates a 400Hz bird chirp
    thru an SPI DAC, once per second. A single protothread
    blinks the LED like a bird's heartbeat.

    GPIO 5 (pin 7) Chip select
    GPIO 6 (pin 9) SCK/spi0_sclk
    GPIO 7 (pin 10) MOSI/spi0_tx
    GPIO 2 (pin 4) GPIO output for timing ISR
    3.3v (pin 36) -> VCC on DAC 
    GND (pin 3)  -> GND on DAC 

 */

// Include necessary libraries
#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
// Include protothreads
#include "pt_cornell_rp2040_v1_4.h"

// Include our modules
#include "dds.h"
#include "dac.h"
#include "audio_envelope.h"
#include "bird_generator.h"

// LED pin
#define LED 25

// Button pin
#define BUTTON_PIN 19

// Cardinal sound key mappings (for keypad integration)
// Key 1: Original swoop sound
// Key 2: Original chirp sound  
// Key 3: Cardinal Linear 1 (7kHz to 4kHz downward sweep - higher range)
// Key 4: Cardinal Silence (brief pause)
// Key 5: Cardinal Linear 2 (2.8kHz to 1.8kHz downward sweep - inverted)
// Key 6: Cardinal Parabola (1.5kHz->2kHz->1.5kHz inverted V-shaped curve)

// This thread runs on core 0
static PT_THREAD (protothread_core_0(struct pt *pt))
{
    // Indicate thread beginning
    PT_BEGIN(pt);
    
    static int sound_counter = 0;
    static bool last_button_state = false;
    
    while(1) {
        
        // Toggle LED like a bird's heartbeat
        gpio_put(LED, !gpio_get(LED));

        // Check for button press (GPIO19)
        bool current_button_state = gpio_get(BUTTON_PIN);
        
        // Detect button press (falling edge - button pressed)
        if (last_button_state && !current_button_state) {
            printf("Button pressed! Playing Cardinal song sequence...\n");
            
            // Play the complete Cardinal song sequence in correct order
            bird_trigger_cardinal_linear_1();    // Key 3: First linear (downward)
            sleep_ms(300);  // Wait for linear 1 to complete
            bird_trigger_cardinal_silence();     // Key 4: Silence
            sleep_ms(100);  // Wait for silence to complete (50ms + buffer)
            bird_trigger_cardinal_linear_2();    // Key 5: Second linear (upward)
            sleep_ms(300);  // Wait for linear 2 to complete
            bird_trigger_cardinal_parabola();    // Key 6: Parabola (V-shaped)
            sleep_ms(300);  // Wait for parabola to complete
        }
        last_button_state = current_button_state;

        // Trigger different bird sounds every 3 seconds (commented out for button control)
        /*
        sound_counter++;
        if (sound_counter >= 60) { // 60 * 50ms = 3 seconds
            sound_counter = 0;
            
            // Alternate between swoop and chirp sounds
            static int sound_type = 0;
            if (sound_type == 0) {
                printf("Playing swoop sound...\n");
                bird_trigger_swoop();
                sound_type = 1;
            } else {
                printf("Playing chirp sound...\n");
                bird_trigger_chirp();
                sound_type = 0;
            }
        }
        */

        // Yield for 50 ms
        PT_YIELD_usec(50000);
    }
    // Indicate thread end
    PT_END(pt);
}

// Core 0 entry point
int main() {
    // Initialize stdio/uart (printf won't work unless you do this!)
    stdio_init_all();
    printf("Hello, bird friends! 🐦\n");

    // Initialize all modules
    dac_init();
    dds_init();
    audio_envelope_init();
    bird_generator_init();

    // Map LED to GPIO port, make it low
    gpio_init(LED);
    gpio_set_dir(LED, GPIO_OUT);
    gpio_put(LED, 0);

    // Initialize button pin (GPIO19) as input with pull-up
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    // Add core 0 threads
    pt_add_thread(protothread_core_0);

    // Start scheduling core 0 threads
    pt_schedule_start;
}
