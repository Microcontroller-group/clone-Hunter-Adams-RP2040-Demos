/**
 * @file sound_recorder.c
 * @brief Sound recording and playback implementation
 * 
 * Implements the recording and playback functionality for keypad sequences.
 * Records key press events with timing and replays them as bird sounds.
 */

#include "sound_recorder.h"
#include "pico/stdlib.h"
#include <stdio.h>

// Include bird generator functions for triggering sounds
#include "bird_generator.h"

// External reference to bird state for checking if sound is complete
extern volatile unsigned int BIRD_STATE_0;

// Forward declaration
static void sound_recorder_play_next_event(void);

// Global recorder instance
sound_recorder_t g_recorder = {
    .state = RECORDER_IDLE,
    .event_count = 0,
    .playback_index = 0,
    .last_playback_time = 0,
    .recording_ready = false
};

void sound_recorder_init(void) {
    // Initialize all events as invalid
    for (int i = 0; i < MAX_RECORDING_LENGTH; i++) {
        g_recorder.events[i].is_valid = false;
        g_recorder.events[i].key_index = -1;
    }
    
    // Reset state
    g_recorder.state = RECORDER_IDLE;
    g_recorder.event_count = 0;
    g_recorder.playback_index = 0;
    g_recorder.last_playback_time = 0;
    g_recorder.recording_ready = false;
    
    printf("Sound recorder initialized\n");
}

void sound_recorder_start_recording(void) {
    if (g_recorder.state != RECORDER_IDLE) {
        printf("Cannot start recording - recorder not idle (state: %d)\n", g_recorder.state);
        return;
    }
    
    // Clear existing recording
    sound_recorder_clear();
    
    // Start recording
    g_recorder.state = RECORDER_RECORDING;
    g_recorder.recording_ready = true;
    
    printf("Recording started - press keys 1-6 to record sounds\n");
}

void sound_recorder_record_key(int key_index) {
    // Only record if we're in recording state and have space
    if (g_recorder.state != RECORDER_RECORDING) {
        return;
    }
    
    // Only record bird sound keys (1-6)
    if (key_index < 1 || key_index > 6) {
        return;
    }
    
    // Check if we have space for more events
    if (g_recorder.event_count >= MAX_RECORDING_LENGTH) {
        printf("Recording buffer full - stopping recording\n");
        sound_recorder_stop_and_play();
        return;
    }
    
    // Record the key press event (no timing needed)
    g_recorder.events[g_recorder.event_count].key_index = key_index;
    g_recorder.events[g_recorder.event_count].is_valid = true;
    g_recorder.event_count++;
    
    printf("Recorded key %d (event %u)\n", key_index, g_recorder.event_count);
}

void sound_recorder_stop_and_play(void) {
    if (g_recorder.state != RECORDER_RECORDING) {
        printf("Cannot stop recording - not currently recording\n");
        return;
    }
    
    // Stop recording
    g_recorder.state = RECORDER_PLAYING;
    g_recorder.playback_index = 0;
    
    printf("Recording stopped - playing back %u events\n", g_recorder.event_count);
    
    // If no events were recorded, just return to idle
    if (g_recorder.event_count == 0) {
        printf("No events recorded - returning to idle\n");
        g_recorder.state = RECORDER_IDLE;
        return;
    }
    
    // Start playing the first event immediately
    sound_recorder_play_next_event();
}

void sound_recorder_update(void) {
    if (g_recorder.state != RECORDER_PLAYING) {
        return;
    }
    
    // Check if we've finished playing all events
    if (g_recorder.playback_index >= g_recorder.event_count) {
        printf("Playback complete - returning to idle\n");
        g_recorder.state = RECORDER_IDLE;
        return;
    }
    
    // Get current time
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    
    // Only play the next event if:
    // 1. The current sound has finished (bird state is IDLE)
    // 2. Enough time has passed since the last sound (for brief pause between sounds)
    if (BIRD_STATE_0 == IDLE && 
        (current_time - g_recorder.last_playback_time) >= PLAYBACK_DELAY_MS) {
        sound_recorder_play_next_event();
    }
}

static void sound_recorder_play_next_event(void) {
    if (g_recorder.playback_index >= g_recorder.event_count) {
        return;
    }
    
    key_press_event_t* event = &g_recorder.events[g_recorder.playback_index];
    
    printf("Playing key %d (event %u of %u)\n", 
           event->key_index, g_recorder.playback_index + 1, g_recorder.event_count);
    
    // Record the time when this sound was triggered
    g_recorder.last_playback_time = to_ms_since_boot(get_absolute_time());
    
    // Trigger the appropriate bird sound based on the recorded key
    switch (event->key_index) {
        case 1:
            bird_trigger_swoop();
            break;
        case 2:
            bird_trigger_chirp();
            break;
        case 3:
            bird_trigger_cardinal_linear_1();
            break;
        case 4:
            bird_trigger_cardinal_silence();
            break;
        case 5:
            bird_trigger_cardinal_linear_2();
            break;
        case 6:
            bird_trigger_cardinal_parabola();
            break;
        default:
            printf("Unknown key index: %d\n", event->key_index);
            break;
    }
    
    // Move to next event
    g_recorder.playback_index++;
}

void sound_recorder_stop(void) {
    printf("Stopping recorder - returning to idle\n");
    g_recorder.state = RECORDER_IDLE;
    g_recorder.playback_index = 0;
    g_recorder.last_playback_time = 0;
    g_recorder.recording_ready = false;
}

bool sound_recorder_is_recording(void) {
    return g_recorder.state == RECORDER_RECORDING;
}

bool sound_recorder_is_playing(void) {
    return g_recorder.state == RECORDER_PLAYING;
}

recorder_state_t sound_recorder_get_state(void) {
    return g_recorder.state;
}

uint32_t sound_recorder_get_event_count(void) {
    return g_recorder.event_count;
}

void sound_recorder_clear(void) {
    // Clear all events
    for (int i = 0; i < MAX_RECORDING_LENGTH; i++) {
        g_recorder.events[i].is_valid = false;
        g_recorder.events[i].key_index = -1;
    }
    
    // Reset counters
    g_recorder.event_count = 0;
    g_recorder.playback_index = 0;
    g_recorder.last_playback_time = 0;
    g_recorder.recording_ready = false;
}
