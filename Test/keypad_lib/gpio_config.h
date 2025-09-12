/**
 * @file gpio_config.h
 * @brief GPIO configuration and initialization module
 * 
 * This module provides functions for configuring GPIO pins used in the
 * keypad demo application.
 */

#ifndef GPIO_CONFIG_H
#define GPIO_CONFIG_H

#include <stdint.h>

// LED pin definition
#define LED_PIN 25

/**
 * @brief Initialize the LED GPIO pin
 * 
 * Configures the LED pin as an output and sets it to low initially.
 */
void gpio_config_led_init(void);

/**
 * @brief Toggle the LED state
 * 
 * Toggles the current state of the LED.
 */
void gpio_config_led_toggle(void);

/**
 * @brief Set the LED state
 * 
 * @param state 1 for on, 0 for off
 */
void gpio_config_led_set(int state);

#endif // GPIO_CONFIG_H
