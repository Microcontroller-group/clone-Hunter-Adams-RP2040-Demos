/**
 * Direct Digital Synthesis (DDS) module
 * Handles sine wave generation using lookup tables
 */

#ifndef DDS_H
#define DDS_H

#include "fixed_point.h"

// DDS parameters
#define two32 4294967296.0  // 2^32 (a constant)
#define Fs 50000
#define sine_table_size 256
#define freq_table_size 407

// DDS state variables
extern volatile unsigned int phase_accum_main_0;
extern volatile unsigned int phase_incr_main_0;
extern fix15 sin_table[sine_table_size];
extern fix15 swoop_table[freq_table_size];
extern fix15 chirp_table[freq_table_size];
extern fix15 cardinal_linear_1_table[freq_table_size];
extern fix15 cardinal_linear_2_table[freq_table_size];
extern fix15 cardinal_parabola_table[freq_table_size];

// Function prototypes
void dds_init(void);
void dds_set_frequency(float frequency);
fix15 dds_get_swoop_frequency(unsigned int index);
fix15 dds_get_chirp_frequency(unsigned int index);
fix15 dds_get_cardinal_linear_1_frequency(unsigned int index);
fix15 dds_get_cardinal_linear_2_frequency(unsigned int index);
fix15 dds_get_cardinal_parabola_frequency(unsigned int index);

#endif // DDS_H
