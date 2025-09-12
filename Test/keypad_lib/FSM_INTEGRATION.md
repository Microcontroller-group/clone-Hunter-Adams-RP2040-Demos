# Keypad FSM Integration

## Overview
The keypad system has been enhanced with a Finite State Machine (FSM) for robust button debouncing and reliable key press/release detection. The FSM implementation follows the same modular design principles as the rest of the system.

## FSM States

The keypad FSM has four states:

1. **`FSM_NOT_PRESSED`** - No key is currently pressed
2. **`FSM_MAYBE_PRESSED`** - A key has been detected, waiting for confirmation
3. **`FSM_PRESSED`** - A key is confirmed as pressed
4. **`FSM_MAYBE_NOT_PRESSED`** - A key has been released, waiting for confirmation

## FSM State Transitions

```
NOT_PRESSED → MAYBE_PRESSED → PRESSED → MAYBE_NOT_PRESSED → NOT_PRESSED
     ↑                                                           ↓
     └─────────────────── (different key pressed) ──────────────┘
```

### Transition Logic:
- **NOT_PRESSED → MAYBE_PRESSED**: When a key is first detected
- **MAYBE_PRESSED → PRESSED**: After debounce threshold is reached with same key
- **MAYBE_PRESSED → NOT_PRESSED**: If key is released before debounce threshold
- **PRESSED → MAYBE_NOT_PRESSED**: When key is released
- **MAYBE_NOT_PRESSED → NOT_PRESSED**: After debounce threshold is reached with no key
- **MAYBE_NOT_PRESSED → PRESSED**: If same key is pressed again before release confirmation
- **Any state → MAYBE_PRESSED**: If a different key is pressed

## Module Structure

### `keypad_fsm.h/c` - FSM Module
**Purpose**: Implements the debouncing FSM for keypad input

**Key Functions**:
- `keypad_fsm_init()` - Initialize FSM with debounce threshold
- `keypad_fsm_update()` - Update FSM with new keypad input
- `keypad_fsm_get_state()` - Get current FSM state
- `keypad_fsm_get_current_key()` - Get currently pressed key
- `keypad_fsm_is_key_pressed()` - Check if key is currently pressed

**FSM Context Structure**:
```c
typedef struct {
    fsm_state_t state;        // Current FSM state
    int current_key;          // Currently pressed key (-1 if none)
    int prev_key;             // Previously pressed key
    int debounce_count;       // Debounce counter
    int debounce_threshold;   // Debounce threshold (configurable)
} keypad_fsm_t;
```

### Updated `display.h/c` - Enhanced Display Module
**New Function**:
- `display_update_key_with_fsm()` - Display key and FSM state information

**Display Information**:
- Current key index
- FSM state name
- Pressed/Released status

### Updated `main.c` - Clean Integration
**FSM Integration**:
- FSM context as static variable in main thread
- FSM initialization on first run
- FSM update on each keypad scan
- Display update with FSM information
- Debug output showing FSM state

## Configuration

### Debounce Threshold
The FSM uses a configurable debounce threshold (default: 3 consecutive readings):
```c
keypad_fsm_init(&fsm, 3); // 3 consecutive readings for debouncing
```

### FSM Benefits

1. **Debouncing**: Eliminates false triggers from mechanical switch bounce
2. **Reliability**: Ensures consistent key press/release detection
3. **State Tracking**: Provides clear state information for debugging
4. **Configurable**: Adjustable debounce threshold for different requirements
5. **Modular**: Clean separation of FSM logic from other concerns

## Usage Example

```c
// Initialize FSM
keypad_fsm_t fsm;
keypad_fsm_init(&fsm, 3);

// In main loop
int current_key = keypad_scan();
int state_changed = keypad_fsm_update(&fsm, current_key);

// Check if key is pressed
if (keypad_fsm_is_key_pressed(&fsm)) {
    int pressed_key = keypad_fsm_get_current_key(&fsm);
    // Handle key press
}
```

## Debug Output

The system provides comprehensive debug output:
```
Key: 5, State: 2, Pressed: 1
```
- **Key**: Current key index (-1 for no key, 0-11 for valid keys)
- **State**: FSM state (0=NOT_PRESSED, 1=MAYBE_PRESSED, 2=PRESSED, 3=MAYBE_NOT_PRESSED)
- **Pressed**: Whether key is currently pressed (1) or not (0)

## VGA Display

The VGA display shows:
- **Key**: Current key index or "---" if none
- **State**: FSM state name
- **Status**: "PRESSED" or "RELEASED"

## Integration with Existing Modules

The FSM integrates seamlessly with the existing modular structure:
- **keypad.h/c**: Provides raw keypad scanning
- **keypad_fsm.h/c**: Adds debouncing and state management
- **display.h/c**: Shows FSM information
- **main.c**: Orchestrates all modules while remaining clean

## Benefits of This Implementation

1. **Clean Architecture**: FSM is a separate module with clear interface
2. **Maintainable**: Easy to modify FSM logic without affecting other modules
3. **Testable**: FSM can be tested independently
4. **Reusable**: FSM module can be used in other projects
5. **Debuggable**: Clear state information for troubleshooting
6. **Configurable**: Adjustable parameters for different requirements

The FSM integration maintains the clean, modular design while adding robust keypad input handling with proper debouncing.
