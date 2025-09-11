# Bird Audio Library

A reusable library for generating bird-like audio beeps (chirps and swoops) on the Raspberry Pi Pico.

Based on Hunter Adams' RP2040 audio synthesis demos, this library provides a clean API for generating sophisticated bird call audio patterns using Direct Digital Synthesis (DDS) and SPI DAC output.

## Features

- **Bird Call Generation**: Alternating chirp and swoop patterns that mimic bird calls
- **Configurable Timing**: Adjustable attack, decay, sustain, and repeat intervals
- **Non-blocking Operation**: Uses timer interrupts for audio generation
- **LED Integration**: Built-in LED blinking thread for visual feedback
- **Protothreads Support**: Integrates with the Cornell protothreads library

## Hardware Requirements

- Raspberry Pi Pico
- SPI DAC (e.g., MCP4822)
- Breadboard and connecting wires

### Pin Connections

| Pico Pin | GPIO | Function | DAC Connection |
|----------|------|----------|----------------|
| 7        | 5    | CS       | Chip Select    |
| 9        | 6    | SCK      | Serial Clock   |
| 10       | 7    | MOSI     | Serial Data    |
| 4        | 2    | Debug    | Optional scope probe |
| 40       | 25   | LED      | Built-in LED   |
| 36       | 3.3V | Power    | VCC            |
| 3        | GND  | Ground   | GND            |

## Usage

### Basic Usage

```c
#include "bird_lib.h"

int main() {
    // Initialize the library
    bird_init();
    
    // Start bird audio generation
    bird_start();
    
    // Add the LED thread to your protothreads
    pt_add_thread(bird_led_thread);
    
    // Start protothreads scheduler
    pt_schedule_start;
}
```

### Advanced Configuration

```c
// Custom timing: attack(5ms), decay(5ms), sustain(120ms), repeat(1250ms)
bird_set_timing(5, 5, 120, 1250);

// Check current state
int state = bird_get_state();
if (state == BIRD_CHIRP) {
    printf("Currently chirping!\n");
}

// Stop audio generation
bird_stop();
```

### Manual Control Mode

The library supports both automatic and manual control modes:

```c
// Switch to manual control mode
bird_set_manual_mode(true);

// Manually trigger sounds
bird_trigger_chirp();  // Start a chirp sound (rising frequency)
bird_trigger_swoop();  // Start a swoop sound (falling frequency)
bird_force_idle();     // Force silence

// Check if in manual mode
if (bird_is_manual_mode()) {
    printf("In manual control mode\n");
}

// Return to automatic mode (resumes automatic chirp/swoop cycling)
bird_set_manual_mode(false);
```

**Manual Mode Behavior:**
- In manual mode, automatic state transitions are disabled
- You control when chirps and swoops start using trigger functions
- Each sound will play its full duration (attack + sustain + decay)
- Sounds don't automatically transition to the next state when finished
- Use `bird_force_idle()` to stop a sound mid-playback

## API Reference

### Functions

#### Core Functions
- `void bird_init(void)` - Initialize the bird audio library
- `void bird_start(void)` - Start bird audio generation
- `void bird_stop(void)` - Stop bird audio generation
- `int bird_get_state(void)` - Get current state (BIRD_IDLE, BIRD_CHIRP, BIRD_SWOOP)
- `void bird_set_timing(int attack_ms, int decay_ms, int sustain_ms, int repeat_ms)` - Configure timing parameters

#### Manual Control Functions
- `void bird_trigger_chirp(void)` - Manually trigger a chirp sound (rising frequency)
- `void bird_trigger_swoop(void)` - Manually trigger a swoop sound (falling frequency)
- `void bird_force_idle(void)` - Force bird to idle state (silence)
- `void bird_set_manual_mode(bool manual_mode)` - Enable/disable manual control mode
- `bool bird_is_manual_mode(void)` - Check if in manual control mode

#### Utility Functions
- `PT_THREAD bird_led_thread(struct pt *pt)` - LED blinking protothread

### States

- `BIRD_IDLE` - No audio generation
- `BIRD_CHIRP` - Generating chirp pattern (rising frequency)
- `BIRD_SWOOP` - Generating swoop pattern (falling frequency)

## Building

Include the bird_library as a subdirectory in your CMakeLists.txt:

```cmake
add_subdirectory(../bird_library bird_library)

target_link_libraries(your_project 
    pico_stdlib 
    bird_lib
    hardware_spi
    hardware_sync
)
```

## Example Project

See the `try_bird_as_library` folder for a complete example demonstrating library usage with interactive button control.
