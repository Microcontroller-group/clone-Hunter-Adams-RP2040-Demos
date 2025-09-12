/**
 * @file keypad.c
 * @brief Keypad scanning and GPIO management implementation
 * 
 * This module implements keypad scanning functionality for a 4x3 matrix keypad
 * connected to the RP2040 GPIO pins.
 */

#include "keypad.h"
#include "pico/stdlib.h"
#include <stdio.h>

// Keypad key codes for 4x3 matrix
const unsigned int keycodes[12] = {   0x28, 0x11, 0x21, 0x41, 0x12,
                                    0x22, 0x42, 0x14, 0x24, 0x44,
                                    0x18, 0x48} ;

const unsigned int scancodes[4] = {   0x01, 0x02, 0x04, 0x08} ;

const unsigned int button = 0x70 ;

// Keypad state variables
char keytext[40];
int prev_key = 0;

void keypad_init(void) {
    // Initialize the keypad GPIO's
    gpio_init_mask((0x7F << BASE_KEYPAD_PIN)) ;
    
    // Set row-pins to output
    gpio_set_dir_out_masked((0xF << BASE_KEYPAD_PIN)) ;
    
    // Set all output pins to low
    gpio_put_masked((0xF << BASE_KEYPAD_PIN), (0x0 << BASE_KEYPAD_PIN)) ;
    
    // Turn on pulldown resistors for column pins (on by default)
    gpio_pull_down((BASE_KEYPAD_PIN + 4)) ;
    gpio_pull_down((BASE_KEYPAD_PIN + 5)) ;
    gpio_pull_down((BASE_KEYPAD_PIN + 6)) ;
}

int keypad_scan(void) {
    static int i;
    static uint32_t keypad;
    
    // Scan the keypad!
    for (i=0; i<KEYROWS; i++) {
        // Set a row high
        gpio_put_masked((0xF << BASE_KEYPAD_PIN),
                        (scancodes[i] << BASE_KEYPAD_PIN)) ;
        // Small delay required
        sleep_us(1) ; 
        // Read the keycode
        keypad = ((gpio_get_all() >> BASE_KEYPAD_PIN) & 0x7F) ;
        // Break if button(s) are pressed
        if (keypad & button) break ;
    }
    
    // If we found a button . . .
    if (keypad & button) {
        // Look for a valid keycode.
        for (i=0; i<NUMKEYS; i++) {
            if (keypad == keycodes[i]) break ;
        }
        // If we don't find one, report invalid keycode
        if (i==NUMKEYS) (i = -1) ;
    }
    // Otherwise, indicate invalid/non-pressed buttons
    else (i=-1) ;
    
    return i;
}

char* keypad_get_key_text(void) {
    return keytext;
}

int keypad_get_prev_key(void) {
    return prev_key;
}

void keypad_set_prev_key(int key) {
    prev_key = key;
}

void keypad_get_key_string(int key, char *buffer, int buffer_size) {
    if (key >= 0 && key < NUMKEYS) {
        snprintf(buffer, buffer_size, "%d", key);
    } else {
        snprintf(buffer, buffer_size, "---");
    }
}