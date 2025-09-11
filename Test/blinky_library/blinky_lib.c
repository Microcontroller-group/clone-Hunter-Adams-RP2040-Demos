/**
 * Blinky Library Implementation
 * A reusable library for blinking LEDs with configurable frequency
 */

#include "blinky_lib.h"
#include "pico/stdlib.h"

// Library state
static uint blinky_led_pin = BLINKY_LED_PIN;
static bool blinky_initialized = false;
static bool blinky_state = false;
static absolute_time_t last_toggle_time;

void blinky_init(uint led_pin) {
    // Use provided pin or default
    if (led_pin != 0) {
        blinky_led_pin = led_pin;
    }
    
    // Initialize the LED pin
    gpio_init(blinky_led_pin);
    // Configure as output
    gpio_set_dir(blinky_led_pin, GPIO_OUT);
    // Start with LED off
    gpio_put(blinky_led_pin, 0);
    blinky_state = false;
    
    // Initialize timing
    last_toggle_time = get_absolute_time();
    blinky_initialized = true;
}

void blinky_start(float frequency_hz) {
    if (!blinky_initialized) {
        blinky_init(0);  // Use default pin
    }
    
    // Calculate half period in milliseconds (for 50% duty cycle)
    uint32_t half_period_ms = (uint32_t)(500.0f / frequency_hz);
    
    // Blocking blink loop
    while (true) {
        // Toggle LED
        blinky_state = !blinky_state;
        gpio_put(blinky_led_pin, blinky_state);
        
        // Sleep for half period
        sleep_ms(half_period_ms);
    }
}

void blinky_stop(void) {
    if (blinky_initialized) {
        gpio_put(blinky_led_pin, 0);
        blinky_state = false;
    }
}

void blinky_update(float frequency_hz) {
    if (!blinky_initialized) {
        blinky_init(0);  // Use default pin
    }
    
    // Calculate half period in microseconds
    uint32_t half_period_us = (uint32_t)(500000.0f / frequency_hz);
    
    // Check if it's time to toggle
    absolute_time_t current_time = get_absolute_time();
    if (absolute_time_diff_us(last_toggle_time, current_time) >= half_period_us) {
        // Toggle LED
        blinky_state = !blinky_state;
        gpio_put(blinky_led_pin, blinky_state);
        
        // Update last toggle time
        last_toggle_time = current_time;
    }
}
