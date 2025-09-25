/**
 * Potentiometer ADC0 demo
 * Reads ADC0 (GPIO26) and prints raw ADC value and millivolts over stdio
 * Based on style of other demos in this repo
 */

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include <stdio.h>

// ADC0 is on GPIO26
#define ADC_PIN 26

int main() {
    // Initialize chosen serial output
    stdio_init_all();

    // Initialize ADC hardware
    adc_init();
    // Make GPIO26 available to ADC
    adc_gpio_init(ADC_PIN);
    // Select ADC input 0 (GPIO26)
    adc_select_input(0);

    // Small delay to allow USB stdio to come up
    sleep_ms(200);

    while (true) {
        // Read ADC (12-bit: 0-4095)
        uint16_t raw = adc_read();
        // Convert to millivolts using 3.3V reference
        uint32_t mV = (uint32_t)raw * 3300 / 4095;
        // Print result over stdio (USB serial)
        printf("ADC0 (GPIO26) raw: %u, %u mV\n", raw, mV);
        sleep_ms(500);
    }
}
