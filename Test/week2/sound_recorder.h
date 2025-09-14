/**
 * @file sound_recorder.h
 * @brief Sound recording and playback library for keypad sequences
 * 
 * This module provides functionality to record sequences of key presses
 * and replay them as bird sounds. Recording starts when KEY_10_INDEX is pressed
 * and stops when KEY_11_INDEX is pressed, which also triggers playback.
 * 
 * The recording system stores:
 * - Key press events with timing information
 * - Maximum sequence length to prevent memory overflow
 * - Playback state management
 */

#ifndef SOUND_RECORDER_H
#define SOUND_RECORDER_H

#include <stdint.h>
#include <stdbool.h>

// Recording configuration
#define MAX_RECORDING_LENGTH 50  // Maximum number of key presses to record
#define PLAYBACK_DELAY_MS 30     // Brief delay between sounds during playback (75ms)

// Recording states
typedef enum {
    RECORDER_IDLE = 0,
    RECORDER_RECORDING,
    RECORDER_PLAYING
} recorder_state_t;

// Key press event structure (simplified - no timing needed)
typedef struct {
    int key_index;           // The key that was pressed (1-6 for bird sounds)
    bool is_valid;           // Whether this event is valid
} key_press_event_t;

// Recording context structure
typedef struct {
    recorder_state_t state;                    // Current recorder state
    key_press_event_t events[MAX_RECORDING_LENGTH];  // Array of recorded events
    uint32_t event_count;                      // Number of events recorded
    uint32_t playback_index;                   // Current playback position
    uint32_t last_playback_time;               // Time when last sound was triggered
    bool recording_ready;                      // Whether recording is ready to start
} sound_recorder_t;

// Global recorder instance
extern sound_recorder_t g_recorder;

/**
 * @brief Initialize the sound recorder
 * 
 * Initializes the recorder to idle state and clears all recorded events.
 */
void sound_recorder_init(void);

/**
 * @brief Start recording a new sequence
 * 
 * Clears any existing recording and starts recording new key press events.
 * Only works if recorder is in IDLE state.
 */
void sound_recorder_start_recording(void);

/**
 * @brief Record a key press event
 * 
 * Records a key press with current timestamp if currently recording.
 * Only records keys 1-6 (bird sound keys).
 * 
 * @param key_index The key that was pressed (1-6 for bird sounds)
 */
void sound_recorder_record_key(int key_index);

/**
 * @brief Stop recording and start playback
 * 
 * Stops recording and immediately starts playing back the recorded sequence.
 * Only works if currently recording.
 */
void sound_recorder_stop_and_play(void);

/**
 * @brief Update the recorder state machine
 * 
 * This function should be called regularly to handle playback timing.
 * Manages the timing between key presses during playback.
 */
void sound_recorder_update(void);

/**
 * @brief Stop any current recording or playback
 * 
 * Immediately stops recording or playback and returns to idle state.
 */
void sound_recorder_stop(void);

/**
 * @brief Check if recorder is currently recording
 * 
 * @return true if recording, false otherwise
 */
bool sound_recorder_is_recording(void);

/**
 * @brief Check if recorder is currently playing
 * 
 * @return true if playing, false otherwise
 */
bool sound_recorder_is_playing(void);

/**
 * @brief Get the current recorder state
 * 
 * @return Current recorder state
 */
recorder_state_t sound_recorder_get_state(void);

/**
 * @brief Get the number of recorded events
 * 
 * @return Number of events in current recording
 */
uint32_t sound_recorder_get_event_count(void);

/**
 * @brief Clear all recorded events
 * 
 * Clears the current recording and returns to idle state.
 */
void sound_recorder_clear(void);

// Legacy function names for compatibility with main.c
#define sound_record() sound_recorder_start_recording()
#define sound_play() sound_recorder_stop_and_play()

#endif // SOUND_RECORDER_H
