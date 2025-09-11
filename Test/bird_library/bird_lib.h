/**
 * Bird Audio Library Header
 * A reusable library for generating bird-like audio beeps (chirps and swoops)
 * Based on Hunter Adams' RP2040 audio synthesis demos
 */

#ifndef BIRD_LIB_H
#define BIRD_LIB_H

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/sync.h"

// Forward declarations - protothreads will be defined by the user application
struct pt;

// Macros for fixed-point arithmetic (faster than floating point)
typedef signed int fix15 ;
#define multfix15(a,b) ((fix15)((((signed long long)(a))*((signed long long)(b)))>>15))
#define float2fix15(a) ((fix15)((a)*32768.0)) 
#define fix2float15(a) ((float)(a)/32768.0)
#define absfix15(a) abs(a) 
#define int2fix15(a) ((fix15)(a << 15))
#define fix2int15(a) ((int)(a >> 15))
#define char2fix15(a) (fix15)(((fix15)(a)) << 15)
#define divfix(a,b) (fix15)( (((signed long long)(a)) << 15) / (b))

// Bird audio states
#define BIRD_IDLE   0
#define BIRD_SWOOP  1
#define BIRD_CHIRP  2

// Default pin configurations (can be overridden)
#ifndef BIRD_PIN_MISO
#define BIRD_PIN_MISO 4
#endif
#ifndef BIRD_PIN_CS
#define BIRD_PIN_CS   5
#endif
#ifndef BIRD_PIN_SCK
#define BIRD_PIN_SCK  6
#endif
#ifndef BIRD_PIN_MOSI
#define BIRD_PIN_MOSI 7
#endif
#ifndef BIRD_LDAC
#define BIRD_LDAC     8
#endif
#ifndef BIRD_LED
#define BIRD_LED      25
#endif
#ifndef BIRD_ISR_GPIO
#define BIRD_ISR_GPIO 2
#endif
#ifndef BIRD_SPI_PORT
#define BIRD_SPI_PORT spi0
#endif

/**
 * Initialize the bird audio library
 * Sets up SPI, GPIO pins, sine tables, and timer interrupts
 */
void bird_init(void);

/**
 * Start bird audio generation
 * This will begin the chirp/swoop cycle
 */
void bird_start(void);

/**
 * Stop bird audio generation
 * Stops the timer interrupt and silences output
 */
void bird_stop(void);

/**
 * Get current bird audio state
 * @return Current state (BIRD_IDLE, BIRD_SWOOP, BIRD_CHIRP)
 */
int bird_get_state(void);

/**
 * Set bird audio parameters
 * @param attack_time_ms Attack time in milliseconds
 * @param decay_time_ms Decay time in milliseconds
 * @param sustain_time_ms Sustain time in milliseconds
 * @param repeat_interval_ms Time between bird calls in milliseconds
 */
void bird_set_timing(int attack_time_ms, int decay_time_ms, int sustain_time_ms, int repeat_interval_ms);

/**
 * Manually trigger a chirp sound (rising frequency)
 * Forces immediate transition to BIRD_CHIRP state
 */
void bird_trigger_chirp(void);

/**
 * Manually trigger a swoop sound (falling frequency)
 * Forces immediate transition to BIRD_SWOOP state
 */
void bird_trigger_swoop(void);

/**
 * Force bird to idle state (silence)
 * Stops current sound generation
 */
void bird_force_idle(void);

/**
 * Set bird to manual control mode
 * @param manual_mode If true, disables automatic state transitions
 */
void bird_set_manual_mode(bool manual_mode);

/**
 * Check if bird is in manual control mode
 * @return true if in manual mode, false if automatic
 */
bool bird_is_manual_mode(void);

/**
 * LED blink update function (call periodically from your main loop)
 * Updates the LED state for visual feedback
 */
void bird_led_update(void);

#endif // BIRD_LIB_H
