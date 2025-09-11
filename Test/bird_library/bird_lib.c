/**
 * Bird Audio Library Implementation
 * A reusable library for generating bird-like audio beeps (chirps and swoops)
 * Based on Hunter Adams' RP2040 audio synthesis demos
 */

#include "bird_lib.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

// Low-level alarm infrastructure we'll be using
#define ALARM_NUM 0
#define ALARM_IRQ TIMER_IRQ_0

//Direct Digital Synthesis (DDS) parameters
#define two32 4294967296.0  // 2^32 (a constant)
#define Fs 40000
#define DELAY 25 // 1/Fs (in microseconds)

// the DDS units - core 0
// Phase accumulator and phase increment. Increment sets output frequency.
static volatile unsigned int phase_accum_main_0;
static volatile unsigned int phase_incr_main_0 = (400.0*two32)/Fs ;
static unsigned int two32Fs = two32/Fs;

static volatile unsigned int freq_swp = 1740;
static volatile unsigned int freq_chp = 2000;

// DDS sine table (populated in bird_init())
#define sine_table_size 256
static fix15 sin_table[sine_table_size] ;
#define freq_table_size 5200
static fix15 swp_table[freq_table_size] ;
static fix15 chp_table[freq_table_size] ;

// Values output to DAC
static int DAC_output_0 ;
static int DAC_output_1 ;

// Amplitude modulation parameters and variables
static fix15 max_amplitude = int2fix15(1) ;    // maximum amplitude
static fix15 attack_inc ;                      // rate at which sound ramps up
static fix15 decay_inc ;                       // rate at which sound ramps down
static fix15 current_amplitude_0 = 0 ;         // current amplitude (modified in ISR)
static fix15 current_amplitude_1 = 0 ;         // current amplitude (modified in ISR)

// Timing parameters for beeps (units of interrupts) - configurable
static int ATTACK_TIME = 200;
static int DECAY_TIME = 200;
static int SUSTAIN_TIME = 4800;
static int BEEP_DURATION = 5200;
static int BEEP_REPEAT_INTERVAL = 50000;

// State machine variables
static volatile unsigned int STATE_0 = BIRD_IDLE ;
static volatile unsigned int count_0 = 0 ;

// SPI data
static uint16_t DAC_data_1 ; // output value
static uint16_t DAC_data_0 ; // output value

// DAC parameters (see the DAC datasheet)
// A-channel, 1x, active
#define DAC_config_chan_A 0b0011000000000000
// B-channel, 1x, active
#define DAC_config_chan_B 0b1011000000000000

// Library initialization flag
static bool bird_initialized = false;

// Manual control mode flag
static bool manual_mode = false;

// This timer ISR is called on core 0
static void bird_alarm_irq(void) {

    // Assert a GPIO when we enter the interrupt
    gpio_put(BIRD_ISR_GPIO, 1) ;

    // Clear the alarm irq
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);

    // Reset the alarm register
    timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + DELAY ;

    if (STATE_0 == BIRD_SWOOP) {
        // DDS phase and sine table lookup
        freq_swp = fix2int15(swp_table[count_0]);
        phase_incr_main_0 = freq_swp * two32Fs;
        phase_accum_main_0 += phase_incr_main_0  ;
        DAC_output_0 = fix2int15(multfix15(current_amplitude_0,
            sin_table[phase_accum_main_0>>24])) + 2048 ;

        // Ramp up amplitude
        if (count_0 < ATTACK_TIME) {
            current_amplitude_0 = (current_amplitude_0 + attack_inc) ;
        }
        // Ramp down amplitude
        else if (count_0 > BEEP_DURATION - DECAY_TIME) {
            current_amplitude_0 = (current_amplitude_0 - decay_inc) ;
        }

        // Mask with DAC control bits
        DAC_data_0 = (DAC_config_chan_B | (DAC_output_0 & 0xffff))  ;

        // SPI write (no spinlock b/c of SPI buffer)
        spi_write16_blocking(BIRD_SPI_PORT, &DAC_data_0, 1) ;

        // Increment the counter
        count_0 += 1 ;

        // State transition (only if not in manual mode)
        if (count_0 == BEEP_DURATION) {
            if (!manual_mode) {
                STATE_0 = BIRD_IDLE ;
            }
            count_0 = 0 ;
        }
    }

    else if (STATE_0 == BIRD_CHIRP) {
        // DDS phase and sine table lookup
        freq_chp = fix2int15(chp_table[count_0]);
        phase_incr_main_0 = freq_chp * two32Fs;
        phase_accum_main_0 += phase_incr_main_0  ;
        DAC_output_0 = fix2int15(multfix15(current_amplitude_0,
            sin_table[phase_accum_main_0>>24])) + 2048 ;

        // Ramp up amplitude
        if (count_0 < ATTACK_TIME) {
            current_amplitude_0 = (current_amplitude_0 + attack_inc) ;
        }
        // Ramp down amplitude
        else if (count_0 > BEEP_DURATION - DECAY_TIME) {
            current_amplitude_0 = (current_amplitude_0 - decay_inc) ;
        }

        // Mask with DAC control bits
        DAC_data_0 = (DAC_config_chan_B | (DAC_output_0 & 0xffff))  ;

        // SPI write (no spinlock b/c of SPI buffer)
        spi_write16_blocking(BIRD_SPI_PORT, &DAC_data_0, 1) ;

        // Increment the counter
        count_0 += 1 ;

        // State transition (only if not in manual mode)
        if (count_0 == BEEP_DURATION) {
            if (!manual_mode) {
                STATE_0 = BIRD_SWOOP ;
            }
            count_0 = 0 ;
        }
    }

    // State transition (IDLE state)?
    else {
        freq_swp = 1740;
        freq_chp = 2000;
        count_0 += 1 ;
        if (count_0 == BEEP_REPEAT_INTERVAL && !manual_mode) {
            current_amplitude_0 = 0 ;
            STATE_0 = BIRD_CHIRP ;
            count_0 = 0 ;
        }
    }

    // De-assert the GPIO when we leave the interrupt
    gpio_put(BIRD_ISR_GPIO, 0) ;
}

void bird_init(void) {
    if (bird_initialized) {
        return; // Already initialized
    }

    // Initialize SPI channel (channel, baud rate set to 20MHz)
    spi_init(BIRD_SPI_PORT, 20000000) ;
    // Format (channel, data bits per transfer, polarity, phase, order)
    spi_set_format(BIRD_SPI_PORT, 16, 0, 0, 0);

    // Map SPI signals to GPIO ports
    gpio_set_function(BIRD_PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(BIRD_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(BIRD_PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(BIRD_PIN_CS, GPIO_FUNC_SPI) ;

    // Map LDAC pin to GPIO port, hold it low (could alternatively tie to GND)
    gpio_init(BIRD_LDAC) ;
    gpio_set_dir(BIRD_LDAC, GPIO_OUT) ;
    gpio_put(BIRD_LDAC, 0) ;

    // Setup the ISR-timing GPIO
    gpio_init(BIRD_ISR_GPIO) ;
    gpio_set_dir(BIRD_ISR_GPIO, GPIO_OUT);
    gpio_put(BIRD_ISR_GPIO, 0) ;

    // Map LED to GPIO port, make it low
    gpio_init(BIRD_LED) ;
    gpio_set_dir(BIRD_LED, GPIO_OUT) ;
    gpio_put(BIRD_LED, 0) ;

    // set up increments for calculating bow envelope
    attack_inc = divfix(max_amplitude, int2fix15(ATTACK_TIME)) ;
    decay_inc =  divfix(max_amplitude, int2fix15(DECAY_TIME)) ;

    // Build the sine lookup table
    // scaled to produce values between 0 and 4096 (for 12-bit DAC)
    int ii;
    for (ii = 0; ii < sine_table_size; ii++){
         sin_table[ii] = float2fix15(2047*sin((float)ii*6.283/(float)sine_table_size));
    }

    // Build frequency sweep table
    int x1;
    for (x1 = 0; x1 < freq_table_size; x1++){
         swp_table[x1] = float2fix15(((float)-1/(float)26000) * (x1 - 2600) * (x1 - 2600) + 2000);
    }

    // Build frequency chirp table
    int x2;
    for (x2 = 0; x2 < freq_table_size; x2++){
         chp_table[x2] = float2fix15(0.000184 * x2 * x2 + 2000);
    }

    bird_initialized = true;
}

void bird_start(void) {
    if (!bird_initialized) {
        bird_init();
    }

    // Enable the interrupt for the alarm (we're using Alarm 0)
    hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM) ;
    // Associate an interrupt handler with the ALARM_IRQ
    irq_set_exclusive_handler(ALARM_IRQ, bird_alarm_irq) ;
    // Enable the alarm interrupt
    irq_set_enabled(ALARM_IRQ, true) ;
    // Write the lower 32 bits of the target time to the alarm register, arming it.
    timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + DELAY ;
}

void bird_stop(void) {
    // Disable the alarm interrupt
    irq_set_enabled(ALARM_IRQ, false) ;
    // Clear any pending interrupt
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);
    // Reset state
    STATE_0 = BIRD_IDLE;
    count_0 = 0;
    current_amplitude_0 = 0;
}

int bird_get_state(void) {
    return STATE_0;
}

void bird_set_timing(int attack_time_ms, int decay_time_ms, int sustain_time_ms, int repeat_interval_ms) {
    // Convert milliseconds to interrupt counts (assuming 40kHz sample rate)
    ATTACK_TIME = (attack_time_ms * Fs) / 1000;
    DECAY_TIME = (decay_time_ms * Fs) / 1000;
    SUSTAIN_TIME = (sustain_time_ms * Fs) / 1000;
    BEEP_DURATION = ATTACK_TIME + SUSTAIN_TIME + DECAY_TIME;
    BEEP_REPEAT_INTERVAL = (repeat_interval_ms * Fs) / 1000;
    
    // Recalculate envelope increments
    attack_inc = divfix(max_amplitude, int2fix15(ATTACK_TIME)) ;
    decay_inc =  divfix(max_amplitude, int2fix15(DECAY_TIME)) ;
}

void bird_trigger_chirp(void) {
    if (!bird_initialized) {
        bird_init();
    }
    
    // Reset counter and amplitude for new sound
    count_0 = 0;
    current_amplitude_0 = 0;
    
    // Force transition to CHIRP state
    STATE_0 = BIRD_CHIRP;
}

void bird_trigger_swoop(void) {
    if (!bird_initialized) {
        bird_init();
    }
    
    // Reset counter and amplitude for new sound
    count_0 = 0;
    current_amplitude_0 = 0;
    
    // Force transition to SWOOP state
    STATE_0 = BIRD_SWOOP;
}

void bird_force_idle(void) {
    // Force transition to IDLE state
    STATE_0 = BIRD_IDLE;
    count_0 = 0;
    current_amplitude_0 = 0;
}

void bird_set_manual_mode(bool manual_mode_enable) {
    manual_mode = manual_mode_enable;
    
    // If disabling manual mode, reset to idle state
    if (!manual_mode) {
        bird_force_idle();
    }
}

bool bird_is_manual_mode(void) {
    return manual_mode;
}

// LED state tracking
static absolute_time_t last_led_toggle_time;
static bool led_initialized = false;

// LED blink update function (call periodically from your main loop)
void bird_led_update(void) {
    if (!led_initialized) {
        last_led_toggle_time = get_absolute_time();
        led_initialized = true;
    }
    
    // Check if 500ms have passed since last toggle
    if (absolute_time_diff_us(last_led_toggle_time, get_absolute_time()) >= 500000) {
        // Toggle LED
        gpio_put(BIRD_LED, !gpio_get(BIRD_LED));
        last_led_toggle_time = get_absolute_time();
    }
}
