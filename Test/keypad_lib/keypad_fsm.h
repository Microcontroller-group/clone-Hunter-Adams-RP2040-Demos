/**
 * @file keypad_fsm.h
 * @brief Keypad Finite State Machine module
 * 
 * This module implements a debouncing FSM for keypad input to handle
 * button press/release events reliably.
 */

#ifndef KEYPAD_FSM_H
#define KEYPAD_FSM_H

#include <stdint.h>

// FSM states for keypad debouncing
typedef enum {
    FSM_NOT_PRESSED,      // No key is pressed
    FSM_MAYBE_PRESSED,    // Key detected, waiting for confirmation
    FSM_PRESSED,          // Key confirmed as pressed
    FSM_MAYBE_NOT_PRESSED // Key released, waiting for confirmation
} fsm_state_t;

// FSM context structure
typedef struct {
    fsm_state_t state;        // Current FSM state
    int current_key;          // Currently pressed key (-1 if none)
    int prev_key;             // Previously pressed key
    int debounce_count;       // Debounce counter
    int debounce_threshold;   // Debounce threshold (configurable)
} keypad_fsm_t;

/**
 * @brief Initialize the keypad FSM
 * 
 * @param fsm Pointer to FSM context structure
 * @param debounce_threshold Number of consecutive readings for debouncing
 */
void keypad_fsm_init(keypad_fsm_t *fsm, int debounce_threshold);

/**
 * @brief Update the FSM with new keypad input
 * 
 * @param fsm Pointer to FSM context structure
 * @param key Current keypad scan result (-1 if no key, 0-11 for valid keys)
 * @return int 1 if key state changed, 0 if no change
 */
int keypad_fsm_update(keypad_fsm_t *fsm, int key);

/**
 * @brief Get the current FSM state
 * 
 * @param fsm Pointer to FSM context structure
 * @return fsm_state_t Current FSM state
 */
fsm_state_t keypad_fsm_get_state(const keypad_fsm_t *fsm);

/**
 * @brief Get the currently pressed key
 * 
 * @param fsm Pointer to FSM context structure
 * @return int Current key (-1 if none pressed)
 */
int keypad_fsm_get_current_key(const keypad_fsm_t *fsm);

/**
 * @brief Get the previously pressed key
 * 
 * @param fsm Pointer to FSM context structure
 * @return int Previous key (-1 if none)
 */
int keypad_fsm_get_prev_key(const keypad_fsm_t *fsm);

/**
 * @brief Check if a key is currently pressed
 * 
 * @param fsm Pointer to FSM context structure
 * @return int 1 if key is pressed, 0 if not
 */
int keypad_fsm_is_key_pressed(const keypad_fsm_t *fsm);

/**
 * @brief Reset the FSM to initial state
 * 
 * @param fsm Pointer to FSM context structure
 */
void keypad_fsm_reset(keypad_fsm_t *fsm);

/**
 * @brief Get FSM state as string
 * 
 * @param state FSM state enum value
 * @return const char* String representation of the state
 */
const char* keypad_fsm_state_to_string(fsm_state_t state);

#endif // KEYPAD_FSM_H
