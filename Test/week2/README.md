# Bird Keypad Demo

This application combines the `bird_lib` and `keypad_lib` libraries to create an interactive bird sound generator controlled by a 4x3 keypad.

## Features

- **Key '1'**: Produces a swoop sound
- **Key '2'**: Produces a chirp sound
- **VGA Display**: Shows current keypad state and pressed keys
- **Serial Output**: Debug information via UART
- **LED Indicator**: Blinks to show system activity

## Hardware Connections

### Keypad (4x3 Matrix)
- GPIO 9   → 330Ω → Pin 1 (button row 1)
- GPIO 10  → 330Ω → Pin 2 (button row 2)
- GPIO 11  → 330Ω → Pin 3 (button row 3)
- GPIO 12  → 330Ω → Pin 4 (button row 4)
- GPIO 13  → Pin 5 (button col 1)
- GPIO 14  → Pin 6 (button col 2)
- GPIO 15  → Pin 7 (button col 3)

### Audio (DAC)
- GPIO 5 (pin 7) → Chip select
- GPIO 6 (pin 9) → SCK/spi0_sclk
- GPIO 7 (pin 10) → MOSI/spi0_tx
- GPIO 2 (pin 4) → GPIO output for timing ISR
- 3.3v (pin 36) → VCC on DAC
- GND (pin 3) → GND on DAC

### VGA Display
- GPIO 16 → VGA Hsync
- GPIO 17 → VGA Vsync
- GPIO 18 → 470Ω resistor → VGA Green
- GPIO 19 → 330Ω resistor → VGA Green
- GPIO 20 → 330Ω resistor → VGA Blue
- GPIO 21 → 330Ω resistor → VGA Red
- RP2040 GND → VGA GND

### Serial Communication
- GPIO 0 → UART RX (white)
- GPIO 1 → UART TX (green)
- RP2040 GND → UART GND

### LED
- GPIO 25 → Built-in LED

## Keypad Layout

The 4x3 keypad uses the following key indices:
```
[0] [1] [2]
[3] [4] [5]
[6] [7] [8]
[9] [*] [#]
```

- Key '1' = Index 0 → Swoop sound
- Key '2' = Index 1 → Chirp sound
- Other keys are detected but don't trigger sounds

## Building and Running

1. **Setup Pico SDK**: Ensure the Pico SDK is installed and `PICO_SDK_PATH` is set.

2. **Build the project**:
   ```bash
   cd Test/week2
   mkdir build
   cd build
   cmake ..
   make
   ```

3. **Flash to RP2040**:
   ```bash
   cp bird_keypad_demo.uf2 /media/your-username/RPI-RP2/
   ```

## Usage

1. Connect the hardware according to the pinout above
2. Power on the RP2040
3. The VGA display will show the keypad state
4. Press key '1' to hear a swoop sound
5. Press key '2' to hear a chirp sound
6. Monitor serial output for debug information

## Technical Details

- **Debouncing**: 3 consecutive readings for reliable key detection
- **Scan Rate**: 30ms intervals
- **Audio Sample Rate**: 50kHz
- **System Clock**: 150MHz (overclocked for better performance)

## Libraries Used

- **bird_lib**: Bird sound generation with DDS and envelope shaping
- **keypad_lib**: 4x3 matrix keypad scanning with FSM debouncing
- **VGA Graphics**: Display system for visual feedback
- **Protothreads**: Cooperative multitasking framework

## Troubleshooting

- **No sound**: Check DAC connections and SPI configuration
- **No keypad response**: Verify GPIO connections and pull-down resistors
- **No VGA display**: Check VGA connections and PIO configuration
- **Serial output issues**: Verify UART connections and baud rate
