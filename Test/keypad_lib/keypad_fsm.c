/**
 * @file keypad_fsm.c
 * @brief Keypad Finite State Machine implementation
 * 
 * This module implements a debouncing FSM for keypad input to handle
 * button press/release events reliably.
 */

#include "keypad_fsm.h"
#include <stdio.h>

void keypad_fsm_init(keypad_fsm_t *fsm, int debounce_threshold) {
    fsm->state = FSM_NOT_PRESSED;
    fsm->current_key = -1;
    fsm->prev_key = -1;
    fsm->debounce_count = 0;
    fsm->debounce_threshold = debounce_threshold;
}

int keypad_fsm_update(keypad_fsm_t *fsm, int key) {
    int state_changed = 0;
    
    // Store previous key for comparison
    fsm->prev_key = fsm->current_key;
    
    switch(fsm->state) {
        case FSM_NOT_PRESSED:
            if (key == -1) {
                // No key pressed, stay in NOT_PRESSED
                fsm->state = FSM_NOT_PRESSED;
                fsm->current_key = -1;
            } else {
                // Key detected, move to MAYBE_PRESSED
                fsm->state = FSM_MAYBE_PRESSED;
                fsm->current_key = key;
                fsm->debounce_count = 1;
            }
            break;
            
        case FSM_MAYBE_PRESSED:
            if (key == fsm->current_key && key != -1) {
                // Same key still pressed, increment debounce counter
                fsm->debounce_count++;
                if (fsm->debounce_count >= fsm->debounce_threshold) {
                    // Debounce threshold reached, confirm key press
                    fsm->state = FSM_PRESSED;
                    state_changed = 1;
                }
            } else if (key == -1) {
                // Key released, go back to NOT_PRESSED
                fsm->state = FSM_NOT_PRESSED;
                fsm->current_key = -1;
                fsm->debounce_count = 0;
            } else {
                // Different key pressed, reset debounce
                fsm->current_key = key;
                fsm->debounce_count = 1;
            }
            break;
            
        case FSM_PRESSED:
            if (key == fsm->current_key && key != -1) {
                // Same key still pressed, stay in PRESSED
                fsm->state = FSM_PRESSED;
            } else if (key == -1) {
                // Key released, move to MAYBE_NOT_PRESSED
                fsm->state = FSM_MAYBE_NOT_PRESSED;
                fsm->debounce_count = 1;
            } else {
                // Different key pressed, handle as new press
                fsm->state = FSM_MAYBE_PRESSED;
                fsm->current_key = key;
                fsm->debounce_count = 1;
            }
            break;
            
        case FSM_MAYBE_NOT_PRESSED:
            if (key == -1) {
                // Still no key, increment debounce counter
                fsm->debounce_count++;
                if (fsm->debounce_count >= fsm->debounce_threshold) {
                    // Debounce threshold reached, confirm key release
                    fsm->state = FSM_NOT_PRESSED;
                    fsm->current_key = -1;
                    fsm->debounce_count = 0;
                    state_changed = 1;
                }
            } else if (key == fsm->current_key) {
                // Same key pressed again, go back to PRESSED
                fsm->state = FSM_PRESSED;
                fsm->debounce_count = 0;
            } else {
                // Different key pressed, handle as new press
                fsm->state = FSM_MAYBE_PRESSED;
                fsm->current_key = key;
                fsm->debounce_count = 1;
            }
            break;
            
        default:
            // Invalid state, reset to NOT_PRESSED
            fsm->state = FSM_NOT_PRESSED;
            fsm->current_key = -1;
            fsm->debounce_count = 0;
            break;
    }
    
    return state_changed;
}

fsm_state_t keypad_fsm_get_state(const keypad_fsm_t *fsm) {
    return fsm->state;
}

int keypad_fsm_get_current_key(const keypad_fsm_t *fsm) {
    return fsm->current_key;
}

int keypad_fsm_get_prev_key(const keypad_fsm_t *fsm) {
    return fsm->prev_key;
}

int keypad_fsm_is_key_pressed(const keypad_fsm_t *fsm) {
    return (fsm->state == FSM_PRESSED) ? 1 : 0;
}

void keypad_fsm_reset(keypad_fsm_t *fsm) {
    fsm->state = FSM_NOT_PRESSED;
    fsm->current_key = -1;
    fsm->prev_key = -1;
    fsm->debounce_count = 0;
}

const char* keypad_fsm_state_to_string(fsm_state_t state) {
    switch(state) {
        case FSM_NOT_PRESSED:
            return "NOT_PRESSED";
        case FSM_MAYBE_PRESSED:
            return "MAYBE_PRESSED";
        case FSM_PRESSED:
            return "PRESSED";
        case FSM_MAYBE_NOT_PRESSED:
            return "MAYBE_NOT_PRESSED";
        default:
            return "UNKNOWN";
    }
}
