/**
 * Direct Digital Synthesis (DDS) implementation
 */

#include "dds.h"
#include <math.h>

// DDS state variables
volatile unsigned int phase_accum_main_0;
volatile unsigned int phase_incr_main_0;
fix15 sin_table[sine_table_size];

void dds_init(void) {
    // Build the sine lookup table
    // scaled to produce values between 0 and 4096 (for 12-bit DAC)
    for (int ii = 0; ii < sine_table_size; ii++) {
        sin_table[ii] = float2fix15(2047*sin((float)ii*6.283/(float)sine_table_size));
    }
    
    // Set default frequency to 400Hz
    dds_set_frequency(400.0);
}

void dds_set_frequency(float frequency) {
    phase_incr_main_0 = (frequency * two32) / Fs;
}
