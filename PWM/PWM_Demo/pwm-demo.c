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

// GPIOs we're using for PWM
#define PWM_PIN1 4
#define PWM_PIN2 5
#define PWM_PIN3 6

// Variables to hold PWM slice numbers
uint slice_num1 ;
uint slice_num3 ;

// PWM duty cycles
volatile int control1 ;
volatile int old_control1 ;
volatile int control2 ;
volatile int old_control2 ;
volatile int control3 ;
volatile int old_control3 ;

// Servo speed in degrees/second
volatile float motor_speed = 60.0;

// Target angle for all motors
volatile int target_angle = 90;

// Stop flag
volatile bool stop_motors = false;

// Test mode flag
volatile bool test_mode = false;

// Current angles for each motor
volatile int angle1 = 90;
volatile int angle2 = 90;
volatile int angle3 = 90;

// PWM interrupt service routine
void on_pwm_wrap() {
    uint32_t status = pwm_get_irq_status_mask();

    // Handle Slice 2 (GPIO 4 and 5)
    if (status & (1u << slice_num1)) {
        pwm_clear_irq(slice_num1);
        
        if (control1 != old_control1) {
            old_control1 = control1;
            pwm_set_chan_level(slice_num1, PWM_CHAN_A, control1);
        }
        if (control2 != old_control2) {
            old_control2 = control2;
            pwm_set_chan_level(slice_num1, PWM_CHAN_B, control2);
        }
    }

    // Handle Slice 3 (GPIO 6)
    if (status & (1u << slice_num3)) {
        pwm_clear_irq(slice_num3);
        
        if (control3 != old_control3) {
            old_control3 = control3;
            pwm_set_chan_level(slice_num3, PWM_CHAN_A, control3);
        }
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

// This thread controls the motors
static PT_THREAD (protothread_motors(struct pt *pt))
{
    PT_BEGIN(pt);

    static int delay_us;
    static const int angle_step = 1;

    while(1) {
        if (motor_speed > 0) {
            delay_us = (int)((angle_step / motor_speed) * 1000000);
        } else {
            delay_us = 100000;
        }

        if (!stop_motors) {
            // Test mode logic: sweep back and forth
            if (test_mode) {
                if (angle1 >= 270) target_angle = 0;
                else if (angle1 <= 0) target_angle = 270;
            }

            // Update Motor 1
            if (angle1 < target_angle) angle1 += angle_step;
            else if (angle1 > target_angle) angle1 -= angle_step;
            control1 = angle_to_duty_cycle(angle1);

            // Update Motor 2
            if (angle2 < target_angle) angle2 += angle_step;
            else if (angle2 > target_angle) angle2 -= angle_step;
            control2 = angle_to_duty_cycle(angle2);

            // Update Motor 3
            if (angle3 < target_angle) angle3 += angle_step;
            else if (angle3 > target_angle) angle3 -= angle_step;
            control3 = angle_to_duty_cycle(angle3);
        }

        PT_YIELD_usec(delay_us);
    }
    PT_END(pt);
}

// User input thread
static PT_THREAD (protothread_serial(struct pt *pt))
{
    PT_BEGIN(pt) ;
    static char cmd[10];
    static float value;
    while(1) {
        sprintf(pt_serial_out_buffer, "Enter 'x', 'speed <val>', 'set <val>', or 'test': ");
        serial_write ;
        // spawn a thread to do the non-blocking serial read
        serial_read ;
        // convert input string to number
        sscanf(pt_serial_in_buffer, "%s %f", cmd, &value);
        
        if (strcmp(cmd, "x") == 0) {
             stop_motors = true;
             test_mode = false;
        } else if (strcmp(cmd, "speed") == 0) {
             if (value > 0) motor_speed = value;
        } else if (strcmp(cmd, "set") == 0) {
             if (value >= 0 && value <= 270) {
                 target_angle = (int)value;
                 stop_motors = false;
                 test_mode = false;
             }
        } else if (strcmp(cmd, "test") == 0) {
             test_mode = true;
             stop_motors = false;
             // Kickstart movement
             if (angle1 >= 270) target_angle = 0;
             else target_angle = 270;
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
    // Tell GPIOs that they are allocated to the PWM
    gpio_set_function(PWM_PIN1, GPIO_FUNC_PWM);
    gpio_set_function(PWM_PIN2, GPIO_FUNC_PWM);
    gpio_set_function(PWM_PIN3, GPIO_FUNC_PWM);

    // Find out which PWM slice is connected to GPIO PWM_OUT (it's slice 2)
    slice_num1 = pwm_gpio_to_slice_num(PWM_PIN1);
    slice_num3 = pwm_gpio_to_slice_num(PWM_PIN3);

    // Mask our slice's IRQ output into the PWM block's single interrupt line,
    // and register our interrupt handler
    pwm_clear_irq(slice_num1);
    pwm_clear_irq(slice_num3);
    pwm_set_irq_enabled(slice_num1, true);
    pwm_set_irq_enabled(slice_num3, true);
    
    irq_set_exclusive_handler(PWM_IRQ_WRAP, on_pwm_wrap);
    irq_set_enabled(PWM_IRQ_WRAP, true);

    // This section configures the period of the PWM signals
    pwm_set_wrap(slice_num1, WRAPVAL) ;
    pwm_set_clkdiv(slice_num1, CLKDIV) ;
    pwm_set_wrap(slice_num3, WRAPVAL) ;
    pwm_set_clkdiv(slice_num3, CLKDIV) ;

    // This sets duty cycle
    pwm_set_chan_level(slice_num1, PWM_CHAN_A, 3125);
    pwm_set_chan_level(slice_num1, PWM_CHAN_B, 3125);
    pwm_set_chan_level(slice_num3, PWM_CHAN_A, 3125);

    // Start the channel
    pwm_set_mask_enabled((1u << slice_num1) | (1u << slice_num3));

    ////////////////////////////////////////////////////////////////////////
    ///////////////////////////// ROCK AND ROLL ////////////////////////////
    ////////////////////////////////////////////////////////////////////////
    pt_add_thread(protothread_motors) ;
    pt_add_thread(protothread_serial) ;
    pt_schedule_start ;

}
