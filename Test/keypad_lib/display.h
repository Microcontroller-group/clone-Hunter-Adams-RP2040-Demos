/**
 * @file display.h
 * @brief VGA display operations module
 * 
 * This module provides a high-level interface for VGA display operations,
 * wrapping the vga16_graphics_v2 library functions.
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include "vga16_graphics_v2.h"

/**
 * @brief Initialize the VGA display system
 * 
 * Initializes the VGA graphics system including PIO state machines,
 * DMA channels, and display buffer.
 */
void display_init(void);

/**
 * @brief Draw the main display layout
 * 
 * Draws the initial display layout including colored boxes and text labels.
 */
void display_draw_layout(void);

/**
 * @brief Update the key display area
 * 
 * Updates the red box area that shows the currently pressed key.
 * 
 * @param key_index The key index to display (0-11) or -1 for no key
 */
void display_update_key(int key_index);

/**
 * @brief Update the key display area with FSM state information
 * 
 * Updates the red box area with key and FSM state information.
 * 
 * @param key_index The key index to display (0-11) or -1 for no key
 * @param fsm_state The current FSM state
 * @param is_pressed Whether the key is currently pressed
 */
void display_update_key_with_fsm(int key_index, int fsm_state, int is_pressed);

/**
 * @brief Clear the key display area
 * 
 * Clears the red box area that shows the currently pressed key.
 */
void display_clear_key(void);

#endif // DISPLAY_H
