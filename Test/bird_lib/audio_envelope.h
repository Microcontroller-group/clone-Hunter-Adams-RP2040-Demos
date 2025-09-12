/**
 * Audio envelope module
 * Handles attack, sustain, and decay phases of audio signals
 */

#ifndef AUDIO_ENVELOPE_H
#define AUDIO_ENVELOPE_H

#include "fixed_point.h"

// Timing parameters for beeps (units of interrupts at 50kHz)
#define ATTACK_TIME             250
#define DECAY_TIME              250
#define SUSTAIN_TIME            6000
#define BEEP_DURATION           6500
#define BEEP_REPEAT_INTERVAL    20000

// Envelope state variables
extern fix15 max_amplitude;
extern fix15 attack_inc;
extern fix15 decay_inc;
extern fix15 current_amplitude_0;

// Function prototypes
void audio_envelope_init(void);
fix15 audio_envelope_update(unsigned int count);

#endif // AUDIO_ENVELOPE_H
