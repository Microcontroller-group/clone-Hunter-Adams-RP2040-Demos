/**
 * @file keypad.h
 * @brief Keypad scanning and GPIO management module
 * 
 * This module provides functions for initializing and scanning a 4x3 keypad
 * connected to the RP2040 GPIO pins.
 * 
 * KEYPAD CONNECTIONS:
 *  - GPIO 9   -->  330 ohms  --> Pin 1 (button row 1)
 *  - GPIO 10  -->  330 ohms  --> Pin 2 (button row 2)
 *  - GPIO 11  -->  330 ohms  --> Pin 3 (button row 3)
 *  - GPIO 12  -->  330 ohms  --> Pin 4 (button row 4)
 *  - GPIO 13  -->     Pin 5 (button col 1)
 *  - GPIO 14  -->     Pin 6 (button col 2)
 *  - GPIO 15  -->     Pin 7 (button col 3)
 */

#ifndef KEYPAD_H
#define KEYPAD_H

#include <stdint.h>

// Keypad configuration constants
#define BASE_KEYPAD_PIN 9
#define KEYROWS         4
#define NUMKEYS         12

// Keypad key codes for 4x3 matrix
extern const unsigned int keycodes[12];
extern const unsigned int scancodes[4];
extern const unsigned int button;

// Keypad state variables
extern char keytext[40];
extern int prev_key;

/**
 * @brief Initialize keypad GPIO pins
 * 
 * Sets up the keypad GPIO pins with proper configuration:
 * - Row pins (9-12) as outputs
 * - Column pins (13-15) as inputs with pulldown resistors
 */
void keypad_init(void);

/**
 * @brief Scan the keypad and return pressed key
 * 
 * Scans through all keypad rows and checks for button presses.
 * Returns the key index (0-11) if a valid key is pressed, -1 otherwise.
 * 
 * @return int Key index (0-11) if pressed, -1 if no key or invalid key
 */
int keypad_scan(void);

/**
 * @brief Get the current keypad state as a string
 * 
 * @return char* String representation of current key state
 */
char* keypad_get_key_text(void);

/**
 * @brief Get the previous key index
 * 
 * @return int Previous key index
 */
int keypad_get_prev_key(void);

/**
 * @brief Set the previous key index
 * 
 * @param key Key index to set
 */
void keypad_set_prev_key(int key);

/**
 * @brief Get keypad scan result as string for display
 * 
 * @param key Key index (-1 for no key, 0-11 for valid keys)
 * @param buffer Buffer to store the string representation
 * @param buffer_size Size of the buffer
 */
void keypad_get_key_string(int key, char *buffer, int buffer_size);

#endif // KEYPAD_H
