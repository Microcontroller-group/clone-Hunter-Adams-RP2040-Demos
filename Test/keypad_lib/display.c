/**
 * @file display.c
 * @brief VGA display operations implementation
 * 
 * This module implements high-level VGA display operations for the keypad demo.
 */

#include "display.h"
#include "keypad_fsm.h"
#include <stdio.h>

void display_init(void) {
    // Initialize the VGA screen
    initVGA();
}

void display_draw_layout(void) {
    // Draw some filled rectangles
    fillRect(64, 0, 176, 50, BLUE); // blue box
    fillRect(250, 0, 176, 50, RED); // red box
    fillRect(435, 0, 176, 50, GREEN); // green box

    // Write some text
    setTextColor(WHITE);
    setCursor(65, 0);
    setTextSize(1);
    writeString("Raspberry Pi Pico");
    setCursor(65, 10);
    writeString("Keypad demo");
    setCursor(65, 20);
    writeString("Hunter Adams");
    setCursor(65, 30);
    writeString("vha3@cornell.edu");
    setCursor(250, 0);
    setTextSize(2);
    writeString("Key Pressed:");
}

void display_update_key(int key_index) {
    char keytext[40];
    
    // Clear the red box area
    fillRect(250, 20, 176, 30, RED);
    
    if (key_index >= 0) {
        // Display the key index
        sprintf(keytext, "%d", key_index);
        setCursor(250, 20);
        setTextSize(2);
        writeString(keytext);
    }
}

void display_update_key_with_fsm(int key_index, int fsm_state, int is_pressed) {
    char keytext[40];
    char statetext[20];
    
    // Clear the red box area
    fillRect(250, 20, 176, 30, RED);
    
    // Display the key index
    if (key_index >= 0) {
        snprintf(keytext, sizeof(keytext), "Key: %d", key_index);
    } else {
        snprintf(keytext, sizeof(keytext), "Key: ---");
    }
    
    // Display FSM state using the FSM module function
    // Convert int state to enum for proper string conversion
    fsm_state_t state_enum = (fsm_state_t)fsm_state;
    snprintf(statetext, sizeof(statetext), "%s", keypad_fsm_state_to_string(state_enum));
    
    // Display key information
    setCursor(250, 20);
    setTextSize(1);
    writeString(keytext);
    
    // Display FSM state
    setCursor(250, 30);
    setTextSize(1);
    writeString(statetext);
    
    // Display pressed status
    setCursor(250, 40);
    setTextSize(1);
    writeString(is_pressed ? "PRESSED" : "RELEASED");
}

void display_clear_key(void) {
    // Clear the red box area
    fillRect(250, 20, 176, 30, RED);
}
