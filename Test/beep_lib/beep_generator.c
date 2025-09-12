/**
 * Beep generator implementation
 */

#include <stdint.h>
#include "beep_generator.h"
#include "hardware/sync.h"

// State machine variables
volatile unsigned int STATE_0 = 0;
volatile unsigned int count_0 = 0;

// Values output to DAC
int DAC_output_0;

void beep_generator_init(void) {
    // Setup the ISR-timing GPIO
    gpio_init(ISR_GPIO);
    gpio_set_dir(ISR_GPIO, GPIO_OUT);
    gpio_put(ISR_GPIO, 0);

    // Enable the interrupt for the alarm (we're using Alarm 0)
    hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);
    // Associate an interrupt handler with the ALARM_IRQ
    irq_set_exclusive_handler(ALARM_IRQ, alarm_irq);
    // Enable the alarm interrupt
    irq_set_enabled(ALARM_IRQ, true);
    // Write the lower 32 bits of the target time to the alarm register, arming it.
    timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + DELAY;
}

// This timer ISR is called on core 0
void alarm_irq(void) {
    // Assert a GPIO when we enter the interrupt
    gpio_put(ISR_GPIO, 1);

    // Clear the alarm irq
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);

    // Reset the alarm register
    timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + DELAY;

    if (STATE_0 == 0) {
        // DDS phase and sine table lookup
        phase_accum_main_0 += phase_incr_main_0;
        DAC_output_0 = fix2int15(multfix15(current_amplitude_0,
            sin_table[phase_accum_main_0>>24])) + 2048;

        // Update envelope
        audio_envelope_update(count_0);

        // Write to DAC
        dac_write_channel_b(DAC_output_0);

        // Increment the counter
        count_0 += 1;

        // State transition?
        if (count_0 == BEEP_DURATION) {
            STATE_0 = 1;
            count_0 = 0;
        }
    }
    // State transition?
    else {
        count_0 += 1;
        if (count_0 == BEEP_REPEAT_INTERVAL) {
            current_amplitude_0 = 0;
            STATE_0 = 0;
            count_0 = 0;
        }
    }

    // De-assert the GPIO when we leave the interrupt
    gpio_put(ISR_GPIO, 0);
}
