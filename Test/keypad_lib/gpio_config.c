/**
 * @file gpio_config.c
 * @brief GPIO configuration and initialization implementation
 * 
 * This module implements GPIO configuration functions for the keypad demo.
 */

#include "gpio_config.h"
#include "pico/stdlib.h"

void gpio_config_led_init(void) {
    // Map LED to GPIO port, make it low
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);
}

void gpio_config_led_toggle(void) {
    gpio_put(LED_PIN, !gpio_get(LED_PIN));
}

void gpio_config_led_set(int state) {
    gpio_put(LED_PIN, state);
}
