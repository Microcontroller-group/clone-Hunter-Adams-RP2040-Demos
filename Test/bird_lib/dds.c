/**
 * Direct Digital Synthesis (DDS) implementation
 */

#include "dds.h"
#include <math.h>

// DDS state variables
volatile unsigned int phase_accum_main_0;
volatile unsigned int phase_incr_main_0;
fix15 sin_table[sine_table_size];
fix15 swoop_table[freq_table_size];
fix15 chirp_table[freq_table_size];
fix15 cardinal_linear_1_table[freq_table_size];
fix15 cardinal_linear_2_table[freq_table_size];
fix15 cardinal_parabola_table[freq_table_size];

void dds_init(void) {
    // Build the sine lookup table
    // scaled to produce values between 0 and 4096 (for 12-bit DAC)
    for (int ii = 0; ii < sine_table_size; ii++) {
        sin_table[ii] = float2fix15(2047*sin((float)ii*6.283/(float)sine_table_size));
    }
    
    // Build frequency modulation lookup tables
    for (int x = 0; x < freq_table_size; x++) {
        swoop_table[x] = float2fix15( 260 * sin((float)3.1415*x/(float)406.25) + 1740 );
    }

    for (int x = 0; x < freq_table_size; x++) {
        chirp_table[x] = float2fix15( 0.03015*x*x + 2000 );
    }
    
    // Cardinal Primitive 1: Inverted Parabola (left half only) - Key 3
    for (int x = 0; x < freq_table_size; x++) {
        float normalized_x = (float)x / freq_table_size;
        // Inverted parabola taking only left half: 8kHz to 2.8kHz
        // Left half of U-curve: starts gradual, then accelerates
        float freq = 8000 - 5200 * (2 * normalized_x - normalized_x * normalized_x);
        cardinal_linear_1_table[x] = float2fix15(freq);
    }
    
    // Cardinal Primitive 2: Second Linear (downward sweep) - Key 5
    for (int x = 0; x < freq_table_size; x++) {
        float normalized_x = (float)x / freq_table_size;
        // Linear downward: 2.8kHz to 1.8kHz (inverted from upward)
        float freq = 2800 - 1000 * normalized_x;
        cardinal_linear_2_table[x] = float2fix15(freq);
    }
    
    // Cardinal Primitive 3: Parabola (inverted V-shaped curve) - Key 6
    for (int x = 0; x < freq_table_size; x++) {
        float normalized_x = (float)x / freq_table_size;
        // Inverted V-shaped curve: 1.5kHz -> 2kHz -> 1.5kHz
        float freq = 1500 + 500 * (4 * normalized_x * (1 - normalized_x));
        cardinal_parabola_table[x] = float2fix15(freq);
    }
    
    // Set default frequency to 400Hz
    dds_set_frequency(400.0);
}

void dds_set_frequency(float frequency) {
    phase_incr_main_0 = (frequency * two32) / Fs;
}

fix15 dds_get_swoop_frequency(unsigned int index) {
    if (index >= freq_table_size) index = freq_table_size - 1;
    return swoop_table[index];
}

fix15 dds_get_chirp_frequency(unsigned int index) {
    if (index >= freq_table_size) index = freq_table_size - 1;
    return chirp_table[index];
}

fix15 dds_get_cardinal_linear_1_frequency(unsigned int index) {
    if (index >= freq_table_size) index = freq_table_size - 1;
    return cardinal_linear_1_table[index];
}

fix15 dds_get_cardinal_linear_2_frequency(unsigned int index) {
    if (index >= freq_table_size) index = freq_table_size - 1;
    return cardinal_linear_2_table[index];
}

fix15 dds_get_cardinal_parabola_frequency(unsigned int index) {
    if (index >= freq_table_size) index = freq_table_size - 1;
    return cardinal_parabola_table[index];
}
