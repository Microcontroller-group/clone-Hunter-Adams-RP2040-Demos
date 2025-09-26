/**
 * @file fsm_button.c
 * @brief A simple 3-state Finite State Machine controlled by a button on a Raspberry Pi Pico.
 *
 * This program cycles through three states whenever a button connected to GP2 is pressed.
 * The current state is printed to the USB serial console.
 *
 * Hardware Setup:
 * - A momentary pushbutton or switch connected between GP2 and any GND pin.
 *
 * Build Instructions:
 * - Create a directory for your project.
 * - Place this file and the CMakeLists.txt file inside it.
 * - Create a 'build' directory.
 * - From the 'build' directory, run:
 * cmake ..
 * make
 * - Drag and drop the resulting 'fsm_button.uf2' file onto your Pico in BOOTSEL mode.
 * - Open a serial monitor (like minicom, screen, or the Thonny serial monitor) to see the output.
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

// Define the GPIO pin for the button
const uint BUTTON_PIN = 2; // Using GP2 for the button

// Define the states for our Finite State Machine (FSM)
typedef enum {
    STATE_ADJUST_BALLS,
    STATE_ADJUST_BOUNCINESS,
    STATE_ADJUST_GRAVITY,
    NUM_STATES // A helper to get the total number of states
} FsmState;

// Array of human-readable names for the states
const char* state_names[] = {
    "Adjust Number of Balls",
    "Adjust Bounciness",
    "Adjust Gravity"
};

// Function to print the current state in a "circled" format
void print_current_state(FsmState current_state) {
    printf("\n----------------------------------\n");
    for (int i = 0; i < NUM_STATES; i++) {
        if (i == current_state) {
            // "Circle" the current state with asterisks
            printf(" ==> (*) %s\n", state_names[i]);
        } else {
            printf("     ( ) %s\n", state_names[i]);
        }
    }
    printf("----------------------------------\n");
}

int main() {
    // Initialize standard I/O for printing to the serial console
    stdio_init_all();

    // Small delay to allow serial monitor to connect
    sleep_ms(2000); 
    printf("--- Pico FSM Program Started ---\n");
    printf("Press the button on GP2 to cycle through states.\n");

    // Initialize the GPIO pin for the button
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN); // Use internal pull-up resistor

    // FSM state variable, starting at the first state
    FsmState current_state = STATE_ADJUST_BALLS;
    
    // Variable to track the last button state for debounce/edge detection
    bool last_button_state = true; // true = released, false = pressed

    // Print the initial state
    print_current_state(current_state);

    // Main loop
    while (true) {
        // Read the current state of the button
        // It will be 'false' (low) when pressed because of the pull-up resistor
        bool button_state = gpio_get(BUTTON_PIN);

        // --- Edge Detection for Button Press ---
        // We only want to change state on the press (falling edge), not while it's held down.
        if (button_state == false && last_button_state == true) {
            
            // Move to the next state
            current_state = (current_state + 1) % NUM_STATES;
            
            // Print the new state to the console
            print_current_state(current_state);
        }

        // Update the last known state of the button
        last_button_state = button_state;
        
        // A small delay to debounce the switch and prevent the loop from running too fast
        sleep_ms(50);
    }

    return 0; // Should never be reached
}
