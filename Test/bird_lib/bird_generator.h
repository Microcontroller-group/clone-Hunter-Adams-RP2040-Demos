/**
 * Bird generator module
 * Main bird chirp generation logic and interrupt service routine
 */

#ifndef BIRD_GENERATOR_H
#define BIRD_GENERATOR_H

#include "fixed_point.h"
#include "dds.h"
#include "dac.h"
#include "audio_envelope.h"

// Low-level alarm infrastructure
#define ALARM_NUM 0
#define ALARM_IRQ TIMER_IRQ_0
#define DELAY 20 // 1/Fs (in microseconds)

// GPIO for timing the ISR
#define ISR_GPIO 2

// State machine variables
extern volatile unsigned int BIRD_STATE_0;
extern volatile unsigned int bird_count_0;

// Function prototypes
void bird_generator_init(void);
void bird_chirp_irq(void);

#endif // BIRD_GENERATOR_H
