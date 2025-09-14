# Sound Recording Library

This library adds recording and playback functionality to the bird keypad demo. It allows you to record sequences of key presses and replay them as bird sounds.

## Features

- **Record key sequences**: Press key '10' (*) to start recording, then press keys 1-6 to record bird sounds
- **Stop and play**: Press key '11' (#) to stop recording and immediately play back the recorded sequence
- **Sequence preservation**: Records the order of key presses (no timing gaps)
- **State management**: Prevents conflicts between recording, playback, and live playing

## Usage

### Recording a Sequence

1. Press key '10' (*) to start recording
   - The system will display "Recording started" message
   - The status will show "REC" in the debug output

2. Press keys 1-6 to record bird sounds:
   - Key 1: Swoop sound
   - Key 2: Chirp sound  
   - Key 3: Cardinal linear 1
   - Key 4: Cardinal silence
   - Key 5: Cardinal linear 2
   - Key 6: Cardinal parabola

3. Press key '11' (#) to stop recording and play back
   - The system will immediately start playing back your recorded sequence
   - The status will show "PLAY" during playback

### During Recording

- All key presses (1-6) are both played live AND recorded
- The system records only the order of key presses (no timing gaps)
- Maximum of 50 key presses can be recorded
- If the buffer fills up, recording stops automatically and playback begins

### During Playback

- The recorded sequence plays back in the same order as recorded
- Sounds play one after another with a brief pause between each sound
- Each sound must complete before the next one starts
- No new key presses are recorded during playback
- Live key presses (1-6) are ignored during playback
- Playback completes automatically and returns to idle state

## API Reference

### Main Functions

- `sound_recorder_init()` - Initialize the recorder (called automatically)
- `sound_recorder_start_recording()` - Start recording a new sequence
- `sound_recorder_stop_and_play()` - Stop recording and start playback
- `sound_recorder_update()` - Update the recorder state machine (called automatically)

### Status Functions

- `sound_recorder_is_recording()` - Returns true if currently recording
- `sound_recorder_is_playing()` - Returns true if currently playing back
- `sound_recorder_get_state()` - Returns current recorder state (IDLE/RECORDING/PLAYING)
- `sound_recorder_get_event_count()` - Returns number of recorded events

### Control Functions

- `sound_recorder_stop()` - Stop any current recording/playback
- `sound_recorder_clear()` - Clear all recorded events

## Technical Details

### Data Structures

The library uses a `sound_recorder_t` structure to manage state:

```c
typedef struct {
    recorder_state_t state;                    // Current state
    key_press_event_t events[MAX_RECORDING_LENGTH];  // Recorded events
    uint32_t event_count;                      // Number of events
    uint32_t playback_index;                   // Current playback position
    uint32_t last_playback_time;               // Time when last sound was triggered
    bool recording_ready;                      // Recording ready flag
} sound_recorder_t;
```

### Key Press Events

Each recorded key press is stored as:

```c
typedef struct {
    int key_index;           // The key pressed (1-6)
    bool is_valid;           // Whether event is valid
} key_press_event_t;
```

### Configuration

- `MAX_RECORDING_LENGTH`: Maximum number of key presses to record (50)
- `PLAYBACK_DELAY_MS`: Brief delay between sounds during playback (200ms)

## Integration

The library is automatically integrated into the main application:

1. **Initialization**: Called in `main()` after bird generator initialization
2. **Key Handling**: Integrated into the keypad FSM update loop
3. **State Updates**: Called in the main protothread loop
4. **Status Display**: Recorder state shown in debug output

## Example Workflow

1. Start the application
2. Press '*' (key 10) - "Recording started"
3. Press '1' - Plays swoop sound and records it
4. Press '2' - Plays chirp sound and records it  
5. Press '3' - Plays cardinal sound and records it
6. Press '#' (key 11) - "Recording stopped - playing back 3 events"
7. System plays back: swoop, chirp, cardinal (in sequence, one after another)
8. Returns to idle state

The recording library provides a seamless way to capture and replay musical sequences using the bird sound generator!
