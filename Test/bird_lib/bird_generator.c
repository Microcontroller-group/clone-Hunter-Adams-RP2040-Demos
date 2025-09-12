/**
 * Bird generator implementation
 * Implements frequency-modulated bird sounds with swoop and chirp patterns
 */

#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "bird_generator.h"
#include "hardware/sync.h"

// State machine variables
volatile unsigned int BIRD_STATE_0 = IDLE;
volatile unsigned int bird_count_0 = 0;

// Values output to DAC
int bird_DAC_output_0;

// Frequency modulation variables
volatile unsigned int freq;
unsigned int two32Fs = two32/Fs;

void bird_generator_init(void) {
    // Setup the ISR-timing GPIO
    gpio_init(ISR_GPIO);
    gpio_set_dir(ISR_GPIO, GPIO_OUT);
    gpio_put(ISR_GPIO, 0);

    // Enable the interrupt for the alarm (we're using Alarm 0)
    hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);
    // Associate an interrupt handler with the ALARM_IRQ
    irq_set_exclusive_handler(ALARM_IRQ, bird_chirp_irq);
    // Enable the alarm interrupt
    irq_set_enabled(ALARM_IRQ, true);
    // Write the lower 32 bits of the target time to the alarm register, arming it.
    timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + DELAY;
}

// This timer ISR is called on core 0 for bird sound generation
void bird_chirp_irq(void) {
    // Assert a GPIO when we enter the interrupt
    gpio_put(ISR_GPIO, 1);

    // Clear the alarm irq
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);

    // Reset the alarm register
    timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + DELAY;

    if (BIRD_STATE_0 == SWOOP) {
        // Frequency modulation table lookup
        freq = fix2int15(dds_get_swoop_frequency(bird_count_0>>4));
        phase_incr_main_0 = freq * two32Fs;
        // DDS phase and sine table lookup
        phase_accum_main_0 += phase_incr_main_0;
        bird_DAC_output_0 = fix2int15(multfix15(current_amplitude_0,
            sin_table[phase_accum_main_0>>24])) + 2048;

        // Update envelope for bird swoop
        audio_envelope_update(bird_count_0);

        // Write to DAC
        dac_write_channel_b(bird_DAC_output_0);

        // Increment the counter
        bird_count_0 += 1;

        // State transition?
        if (bird_count_0 == BEEP_DURATION) {
            BIRD_STATE_0 = IDLE;
            bird_count_0 = 0;
        }
    } else if (BIRD_STATE_0 == CHIRP) {
        // Frequency modulation table lookup
        freq = fix2int15(dds_get_chirp_frequency(bird_count_0>>4));
        phase_incr_main_0 = freq * two32Fs;
        // DDS phase and sine table lookup
        phase_accum_main_0 += phase_incr_main_0;
        bird_DAC_output_0 = fix2int15(multfix15(current_amplitude_0,
            sin_table[phase_accum_main_0>>24])) + 2048;

        // Update envelope for bird chirp
        audio_envelope_update(bird_count_0);

        // Write to DAC
        dac_write_channel_b(bird_DAC_output_0);

        // Increment the counter
        bird_count_0 += 1;

        // State transition?
        if (bird_count_0 == BEEP_DURATION) {
            BIRD_STATE_0 = IDLE;
            bird_count_0 = 0;
        }
    } else {
        // IDLE state - no sound
        current_amplitude_0 = 0;
    }

    // De-assert the GPIO when we leave the interrupt
    gpio_put(ISR_GPIO, 0);
}

// Trigger a swoop sound
void bird_trigger_swoop(void) {
    if (BIRD_STATE_0 == IDLE) {
        BIRD_STATE_0 = SWOOP;
        bird_count_0 = 0;
        current_amplitude_0 = 0;
    }
}

// Trigger a chirp sound
void bird_trigger_chirp(void) {
    if (BIRD_STATE_0 == IDLE) {
        BIRD_STATE_0 = CHIRP;
        bird_count_0 = 0;
        current_amplitude_0 = 0;
    }
}

// Stop any currently playing sound
void bird_stop_sound(void) {
    BIRD_STATE_0 = IDLE;
    bird_count_0 = 0;
    current_amplitude_0 = 0;
}
