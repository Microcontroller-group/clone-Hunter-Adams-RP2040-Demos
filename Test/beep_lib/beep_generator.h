/**
 * Beep generator module
 * Main beep generation logic and interrupt service routine
 */

#ifndef BEEP_GENERATOR_H
#define BEEP_GENERATOR_H

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
extern volatile unsigned int STATE_0;
extern volatile unsigned int count_0;

// Function prototypes
void beep_generator_init(void);
void alarm_irq(void);

#endif // BEEP_GENERATOR_H
