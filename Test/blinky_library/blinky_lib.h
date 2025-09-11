/**
 * Blinky Library Header
 * A reusable library for blinking LEDs with configurable frequency
 */

#ifndef BLINKY_LIB_H
#define BLINKY_LIB_H

#include "pico/stdlib.h"

// Default LED pin (can be overridden)
#ifndef BLINKY_LED_PIN
#define BLINKY_LED_PIN 25
#endif

/**
 * Initialize the blinky library
 * @param led_pin GPIO pin number for the LED (use BLINKY_LED_PIN if 0)
 */
void blinky_init(uint led_pin);

/**
 * Start blinking with specified frequency
 * @param frequency_hz Frequency in Hz (blinks per second)
 */
void blinky_start(float frequency_hz);

/**
 * Stop blinking (LED will be turned off)
 */
void blinky_stop(void);

/**
 * Single blink cycle (non-blocking)
 * Call this repeatedly in your main loop
 * @param frequency_hz Frequency in Hz
 */
void blinky_update(float frequency_hz);

#endif // BLINKY_LIB_H
