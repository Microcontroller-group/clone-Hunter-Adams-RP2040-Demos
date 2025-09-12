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

// This thread runs on core 0
static PT_THREAD (protothread_core_0(struct pt *pt))
{
    // Indicate thread beginning
    PT_BEGIN(pt);
    while(1) {
        
        // Toggle LED like a bird's heartbeat
        gpio_put(LED, !gpio_get(LED));

        // Yield for 500 ms
        PT_YIELD_usec(500000);
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

    // Add core 0 threads
    pt_add_thread(protothread_core_0);

    // Start scheduling core 0 threads
    pt_schedule_start;
}
