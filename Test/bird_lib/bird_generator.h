/**
 * Bird generator module
 * Main bird chirp generation logic and interrupt service routine
 */

#ifndef BIRD_GENERATOR_H
#define BIRD_GENERATOR_H

#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "fixed_point.h"
#include "dds.h"
#include "dac.h"
#include "audio_envelope.h"

// Low-level alarm infrastructure
#define ALARM_NUM 0
#define ALARM_IRQ TIMER_IRQ_0
#define DELAY 20 // 1/Fs (in microseconds) - 1/50000 = 20μs

// GPIO for timing the ISR
#define ISR_GPIO 2

// Bird sound states
#define IDLE   0
#define SWOOP  1
#define CHIRP  2

// State machine variables
extern volatile unsigned int BIRD_STATE_0;
extern volatile unsigned int bird_count_0;

// Function prototypes
void bird_generator_init(void);
void bird_chirp_irq(void);
void bird_trigger_swoop(void);
void bird_trigger_chirp(void);
void bird_stop_sound(void);

#endif // BIRD_GENERATOR_H
