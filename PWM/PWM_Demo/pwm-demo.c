/**
 * V. Hunter Adams (vha3@cornell.edu)
 * PWM demo code with serial input
 * 
 * This demonstration sets a PWM duty cycle to a
 * user-specified value.
 * 
 * HARDWARE CONNECTIONS
 *   - GPIO 4 ---> PWM output
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "hardware/pwm.h"
#include "hardware/irq.h"

#include "pt_cornell_rp2040_v1_4.h"

// PWM wrap value and clock divide value
// For a CPU rate of 125 MHz, this gives
// a PWM frequency of 50 Hz.
#define WRAPVAL 24999
#define CLKDIV 100.0f

// GPIO we're using for PWM
#define PWM_OUT 4

// Variable to hold PWM slice number
uint slice_num ;

// PWM duty cycle
volatile int control ;
volatile int old_control ;
// Servo sweep speed in degrees/second
volatile float sweep_speed = 100.0;

// PWM interrupt service routine
void on_pwm_wrap() {
    // Clear the interrupt flag that brought us here
    pwm_clear_irq(pwm_gpio_to_slice_num(PWM_OUT));
    // Update duty cycle
    if (control!=old_control) {
        old_control = control ;
        pwm_set_chan_level(slice_num, PWM_CHAN_A, control);
    }
}

// This function maps an angle (0-270 degrees) to a PWM duty cycle.
int angle_to_duty_cycle(int angle) {
    // The range of duty cycles that correspond to 0-270 degrees
    static const int min_duty = 625;
    static const int max_duty = 3125;
    // The range of angles
    static const int min_angle = 0;
    static const int max_angle = 270;

    // Linearly map the angle to the duty cycle
    return min_duty + (int)(((float)(angle - min_angle) / (max_angle - min_angle)) * (max_duty - min_duty));
}

// This thread sweeps the servo back and forth
static PT_THREAD (protothread_sweep(struct pt *pt))
{
    PT_BEGIN(pt);

    // The range of angles
    static const int min_angle = 0;
    static const int max_angle = 270;
    // The amount to change the angle by in each step
    static const int angle_step = 1;
    
    static int angle = 0;
    static int sweep_delay_us;

    while(1) {
        // Calculate delay based on current speed.
        if (sweep_speed > 0) {
            sweep_delay_us = (int)((angle_step / sweep_speed) * 1000000);
        } else {
            // If speed is 0 or negative, don't move
            PT_YIELD_usec(100000); // Yield for a bit to prevent busy-waiting
            continue;
        }

        // Sweep from 0 to 270 degrees
        for (angle = min_angle; angle <= max_angle; angle += angle_step) {
            control = angle_to_duty_cycle(angle);
            PT_YIELD_usec(sweep_delay_us);
        }

        // Sweep from 270 to 0 degrees
        for (angle = max_angle; angle >= min_angle; angle -= angle_step) {
            control = angle_to_duty_cycle(angle);
            PT_YIELD_usec(sweep_delay_us);
        }
    }
    PT_END(pt);
}

// User input thread
static PT_THREAD (protothread_serial(struct pt *pt))
{
    PT_BEGIN(pt) ;
    static float new_speed ;
    while(1) {
        sprintf(pt_serial_out_buffer, "Input a sweep speed (degrees/sec): ");
        serial_write ;
        // spawn a thread to do the non-blocking serial read
        serial_read ;
        // convert input string to number
        sscanf(pt_serial_in_buffer,"%f", &new_speed) ;
        if (new_speed >= 0) {
            sweep_speed = new_speed;
        }
    }
    PT_END(pt) ;
}

int main() {

    // Initialize stdio
    stdio_init_all();

    ////////////////////////////////////////////////////////////////////////
    ///////////////////////// PWM CONFIGURATION ////////////////////////////
    ////////////////////////////////////////////////////////////////////////
    // Tell GPIO PWM_OUT that it is allocated to the PWM
    gpio_set_function(PWM_OUT, GPIO_FUNC_PWM);

    // Find out which PWM slice is connected to GPIO PWM_OUT (it's slice 2)
    slice_num = pwm_gpio_to_slice_num(PWM_OUT);

    // Mask our slice's IRQ output into the PWM block's single interrupt line,
    // and register our interrupt handler
    pwm_clear_irq(slice_num);
    pwm_set_irq_enabled(slice_num, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, on_pwm_wrap);
    irq_set_enabled(PWM_IRQ_WRAP, true);

    // This section configures the period of the PWM signals
    pwm_set_wrap(slice_num, WRAPVAL) ;
    pwm_set_clkdiv(slice_num, CLKDIV) ;

    // This sets duty cycle
    pwm_set_chan_level(slice_num, PWM_CHAN_A, 3125);

    // Start the channel
    pwm_set_mask_enabled((1u << slice_num));

    ////////////////////////////////////////////////////////////////////////
    ///////////////////////////// ROCK AND ROLL ////////////////////////////
    ////////////////////////////////////////////////////////////////////////
    pt_add_thread(protothread_sweep) ;
    pt_add_thread(protothread_serial) ;
    pt_schedule_start ;

}
