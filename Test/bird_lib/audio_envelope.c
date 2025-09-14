/**
 * Audio envelope implementation
 */

#include "audio_envelope.h"

// Envelope state variables
fix15 max_amplitude = int2fix15(1);    // maximum amplitude
fix15 attack_inc;                      // rate at which sound ramps up
fix15 decay_inc;                       // rate at which sound ramps down
fix15 current_amplitude_0 = 0;         // current amplitude (modified in ISR)

void audio_envelope_init(void) {
    // Set up increments for calculating envelope
    attack_inc = divfix(max_amplitude, int2fix15(ATTACK_TIME));
    decay_inc = divfix(max_amplitude, int2fix15(DECAY_TIME));
}

fix15 audio_envelope_update(unsigned int count) {
    // Ramp up amplitude
    if (count < ATTACK_TIME) {
        current_amplitude_0 = (current_amplitude_0 + attack_inc);
    }
    // Ramp down amplitude
    else if (count > BEEP_DURATION - DECAY_TIME) {
        current_amplitude_0 = (current_amplitude_0 - decay_inc);
    }
    
    return current_amplitude_0;
}

fix15 audio_envelope_update_cardinal_linear_1(unsigned int count) {
    // Ramp up amplitude
    if (count < ATTACK_TIME) {
        current_amplitude_0 = (current_amplitude_0 + attack_inc);
    }
    // Ramp down amplitude - use the correct duration for cardinal linear 1
    else if (count > CARDINAL_LINEAR_1_DURATION - DECAY_TIME) {
        current_amplitude_0 = (current_amplitude_0 - decay_inc);
    }
    
    return current_amplitude_0;
}
