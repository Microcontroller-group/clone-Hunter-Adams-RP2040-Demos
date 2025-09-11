/**
 * Demo Library Usage Project - Using Blinky Library
 * Demonstrates how to use the blinky library with configurable frequency
 */

#include "pico/stdlib.h"
#include "blinky_lib.h"

// Define the blink frequency (1 Hz = 1 blink per second)
#define BLINK_FREQUENCY_HZ 5.0f

int main() {
    // Initialize the blinky library with default LED pin (GPIO 25)
    blinky_init(0);  // 0 means use default pin
    
    // Option 1: Use blocking blink (uncomment to use)
    // blinky_start(BLINK_FREQUENCY_HZ);
    
    // Option 2: Use non-blocking blink in main loop
    while (true) {
        blinky_update(BLINK_FREQUENCY_HZ);
        // You can do other work here since blinky_update() is non-blocking
        sleep_ms(10);  // Small delay to prevent busy waiting
    }
    
    return 0;
}
