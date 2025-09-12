# FSM States Reference

## State Meanings

### 1. **NOT_PRESSED** (State: 0)
- **Meaning**: No key is currently pressed
- **When it occurs**: Initial state, or after a key has been fully released
- **Debug output**: `Key: -1, State: NOT_PRESSED, Pressed: NO`

### 2. **MAYBE_PRESSED** (State: 1) 
- **Meaning**: A key has been detected, but we're waiting for confirmation (debouncing)
- **When it occurs**: When a key is first detected, before debounce threshold is reached
- **Debug output**: `Key: 5, State: MAYBE_PRESSED, Pressed: NO`

### 3. **PRESSED** (State: 2)
- **Meaning**: A key is confirmed as pressed (debounce threshold reached)
- **When it occurs**: After the same key has been detected for the debounce threshold count
- **Debug output**: `Key: 5, State: PRESSED, Pressed: YES`

### 4. **MAYBE_NOT_PRESSED** (State: 3)
- **Meaning**: A key has been released, but we're waiting for confirmation (debouncing)
- **When it occurs**: When a pressed key is released, before debounce threshold is reached
- **Debug output**: `Key: 5, State: MAYBE_NOT_PRESSED, Pressed: NO`

## State Transition Examples

### Normal Key Press:
```
NOT_PRESSED → MAYBE_PRESSED → PRESSED
Key: -1, State: NOT_PRESSED, Pressed: NO
Key: 5, State: MAYBE_PRESSED, Pressed: NO
Key: 5, State: PRESSED, Pressed: YES
```

### Normal Key Release:
```
PRESSED → MAYBE_NOT_PRESSED → NOT_PRESSED
Key: 5, State: PRESSED, Pressed: YES
Key: 5, State: MAYBE_NOT_PRESSED, Pressed: NO
Key: -1, State: NOT_PRESSED, Pressed: NO
```

### Quick Key Press (before debounce):
```
NOT_PRESSED → MAYBE_PRESSED → NOT_PRESSED
Key: -1, State: NOT_PRESSED, Pressed: NO
Key: 5, State: MAYBE_PRESSED, Pressed: NO
Key: -1, State: NOT_PRESSED, Pressed: NO
```

## Debug Output Format

**New Format** (with meaningful names):
```
Key: 5, State: PRESSED, Pressed: YES
```

**Old Format** (numbers only):
```
Key: 5, State: 2, Pressed: 1
```

## VGA Display

The VGA display now shows:
- **Key**: Current key index or "---" if none
- **State**: Full state name (e.g., "NOT_PRESSED", "MAYBE_PRESSED")
- **Status**: "PRESSED" or "RELEASED"

## Configuration

- **Debounce Threshold**: 3 consecutive readings (configurable)
- **Scan Rate**: 30ms intervals
- **State Persistence**: States persist until transition conditions are met
