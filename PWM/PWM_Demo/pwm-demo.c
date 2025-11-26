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

// Servo sweep speed in degrees/second
volatile float sweep_speed = 100.0;
// Active motor selection (0=all, 1=motor1, 2=motor2, 3=motor3)
volatile int active_motor = 0;

// Control mode (0=sweep)
#define MODE_SWEEP 0
#define MODE_RESET 1
#define MODE_MOVE_1_2 2
#define MODE_WALK 3
volatile int control_mode = MODE_SWEEP;

// Independent angles and directions for each motor
volatile int angle1 = 0;
volatile int dir1 = 1;

volatile int angle2 = 90;
volatile int dir2 = 1;

volatile int angle3 = 180;
volatile int dir3 = 1;

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

// Target reset angle
volatile int reset_target_angle = 0;

// Reset all motors to target angle
void motor_reset(int target) {
    if (target >= 0 && target <= 270) {
        reset_target_angle = target;
        control_mode = 1;
    }
}

// Move motor 1 and 2 to target angle
void motor_move_1_2(int target) {
    if (target >= 0 && target <= 270) {
        reset_target_angle = target;
        control_mode = 2;
    }
}

// Walk mode state
volatile int walk_state = 0;

// Start walk mode
void motor_walk() {
    walk_state = 0;
    control_mode = 3;
}

// Reset mode step function
int mode_reset_step() {
    static const int angle_step = 1;
    int delay_us;

    if (sweep_speed > 0) {
        delay_us = (int)((angle_step / sweep_speed) * 1000000);
    } else {
        delay_us = 10000;
    }

    int active = 0;

    // Move Motor 1 towards target
    if (angle1 > reset_target_angle) { angle1 -= angle_step; if (angle1 < reset_target_angle) angle1 = reset_target_angle; active = 1; }
    else if (angle1 < reset_target_angle) { angle1 += angle_step; if (angle1 > reset_target_angle) angle1 = reset_target_angle; active = 1; }
    control1 = angle_to_duty_cycle(angle1);

    // Move Motor 2 towards target
    if (angle2 > reset_target_angle) { angle2 -= angle_step; if (angle2 < reset_target_angle) angle2 = reset_target_angle; active = 1; }
    else if (angle2 < reset_target_angle) { angle2 += angle_step; if (angle2 > reset_target_angle) angle2 = reset_target_angle; active = 1; }
    control2 = angle_to_duty_cycle(angle2);

    // Move Motor 3 towards target
    if (angle3 > reset_target_angle) { angle3 -= angle_step; if (angle3 < reset_target_angle) angle3 = reset_target_angle; active = 1; }
    else if (angle3 < reset_target_angle) { angle3 += angle_step; if (angle3 > reset_target_angle) angle3 = reset_target_angle; active = 1; }
    control3 = angle_to_duty_cycle(angle3);

    if (!active) {
        dir1 = 1; dir2 = 1; dir3 = 1;
    }

    return delay_us;
}

// Move 1 & 2 mode step function
int mode_move_1_2_step() {
    static const int angle_step = 1;
    int delay_us;

    if (sweep_speed > 0) {
        delay_us = (int)((angle_step / sweep_speed) * 1000000);
    } else {
        delay_us = 10000;
    }

    int active = 0;

    // Move Motor 1 towards target
    if (angle1 > reset_target_angle) { angle1 -= angle_step; if (angle1 < reset_target_angle) angle1 = reset_target_angle; active = 1; }
    else if (angle1 < reset_target_angle) { angle1 += angle_step; if (angle1 > reset_target_angle) angle1 = reset_target_angle; active = 1; }
    control1 = angle_to_duty_cycle(angle1);

    // Move Motor 2 towards target
    if (angle2 > reset_target_angle) { angle2 -= angle_step; if (angle2 < reset_target_angle) angle2 = reset_target_angle; active = 1; }
    else if (angle2 < reset_target_angle) { angle2 += angle_step; if (angle2 > reset_target_angle) angle2 = reset_target_angle; active = 1; }
    control2 = angle_to_duty_cycle(angle2);

    if (!active) {
        dir1 = 1; dir2 = 1;
    }

    return delay_us;
}

// Sweep mode step function
int mode_sweep_step() {
    static const int min_angle = 0;
    static const int max_angle = 270;
    static const int angle_step = 1;

    int delay_us;

    if (sweep_speed > 0) {
        delay_us = (int)((angle_step / sweep_speed) * 1000000);
    } else {
        return 100000;
    }

    // Update Motor 1
    if (active_motor == 0 || active_motor == 1) {
        angle1 += dir1 * angle_step;
        if (angle1 >= max_angle) { angle1 = max_angle; dir1 = -1; }
        else if (angle1 <= min_angle) { angle1 = min_angle; dir1 = 1; }
        control1 = angle_to_duty_cycle(angle1);
    }

    // Update Motor 2
    if (active_motor == 0 || active_motor == 2) {
        angle2 += dir2 * angle_step;
        if (angle2 >= max_angle) { angle2 = max_angle; dir2 = -1; }
        else if (angle2 <= min_angle) { angle2 = min_angle; dir2 = 1; }
        control2 = angle_to_duty_cycle(angle2);
    }

    // Update Motor 3
    if (active_motor == 0 || active_motor == 3) {
        angle3 += dir3 * angle_step;
        if (angle3 >= max_angle) { angle3 = max_angle; dir3 = -1; }
        else if (angle3 <= min_angle) { angle3 = min_angle; dir3 = 1; }
        control3 = angle_to_duty_cycle(angle3);
    }

    return delay_us;
}

// Walk mode step function
int mode_walk_step() {
    static const int angle_step = 1;
    int delay_us;

    if (sweep_speed > 0) {
        delay_us = (int)((angle_step / sweep_speed) * 1000000);
    } else {
        delay_us = 10000;
    }

    int active = 0;
    int target = 0;

    switch (walk_state) {
        case 0: // Init: Move all to 90
            target = 90;
            if (angle1 < target) { angle1 += angle_step; if (angle1 > target) angle1 = target; active = 1; }
            else if (angle1 > target) { angle1 -= angle_step; if (angle1 < target) angle1 = target; active = 1; }
            
            if (angle2 < target) { angle2 += angle_step; if (angle2 > target) angle2 = target; active = 1; }
            else if (angle2 > target) { angle2 -= angle_step; if (angle2 < target) angle2 = target; active = 1; }
            
            if (angle3 < target) { angle3 += angle_step; if (angle3 > target) angle3 = target; active = 1; }
            else if (angle3 > target) { angle3 -= angle_step; if (angle3 < target) angle3 = target; active = 1; }
            
            if (!active) walk_state = 1;
            break;

        case 1: // Wait 1s
            walk_state = 2;
            return 1000000;

        case 2: // Move 1 & 2 to 105
            target = 120;
            if (angle1 < target) { angle1 += angle_step; if (angle1 > target) angle1 = target; active = 1; }
            else if (angle1 > target) { angle1 -= angle_step; if (angle1 < target) angle1 = target; active = 1; }
            
            if (angle2 < target) { angle2 += angle_step; if (angle2 > target) angle2 = target; active = 1; }
            else if (angle2 > target) { angle2 -= angle_step; if (angle2 < target) angle2 = target; active = 1; }
            
            if (!active) walk_state = 3;
            break;

        case 3: // Wait 0.5s
            walk_state = 4;
            return 500000;

        case 4: // Move 1 & 2 to 75
            target = 60;
            if (angle1 < target) { angle1 += angle_step; if (angle1 > target) angle1 = target; active = 1; }
            else if (angle1 > target) { angle1 -= angle_step; if (angle1 < target) angle1 = target; active = 1; }
            
            if (angle2 < target) { angle2 += angle_step; if (angle2 > target) angle2 = target; active = 1; }
            else if (angle2 > target) { angle2 -= angle_step; if (angle2 < target) angle2 = target; active = 1; }
            
            if (!active) walk_state = 5;
            break;

        case 5: // Wait 0.5s
            walk_state = 6;
            return 500000;

        case 6: // Move 1 & 2 to 90
            target = 90;
            if (angle1 < target) { angle1 += angle_step; if (angle1 > target) angle1 = target; active = 1; }
            else if (angle1 > target) { angle1 -= angle_step; if (angle1 < target) angle1 = target; active = 1; }
            
            if (angle2 < target) { angle2 += angle_step; if (angle2 > target) angle2 = target; active = 1; }
            else if (angle2 > target) { angle2 -= angle_step; if (angle2 < target) angle2 = target; active = 1; }
            
            if (!active) walk_state = 7;
            break;

        case 7: // Wait 0.5s
            walk_state = 2; // Repeat from step [1] (which is state 2 here)
            return 500000;
    }

    control1 = angle_to_duty_cycle(angle1);
    control2 = angle_to_duty_cycle(angle2);
    control3 = angle_to_duty_cycle(angle3);

    return delay_us;
}

// This thread controls the motors based on the selected mode
static PT_THREAD (protothread_motors(struct pt *pt))
{
    PT_BEGIN(pt);

    static int delay_us;

    while(1) {
        if (control_mode == MODE_SWEEP) {
            delay_us = mode_sweep_step();
        } else if (control_mode == MODE_RESET) {
            delay_us = mode_reset_step();
        } else if (control_mode == MODE_MOVE_1_2) {
            delay_us = mode_move_1_2_step();
        } else if (control_mode == MODE_MOVE_1_2) {
            delay_us = mode_move_1_2_step();
        } else if (control_mode == MODE_WALK) {
            delay_us = mode_walk_step();
        } else {
            // Default safe state or other modes
            delay_us = 100000;
        }
        PT_YIELD_usec(delay_us);
    }
    PT_END(pt);
}

// User input thread
static PT_THREAD (protothread_serial(struct pt *pt))
{
    PT_BEGIN(pt) ;
    static char cmd;
    static float value;
    while(1) {
        sprintf(pt_serial_out_buffer, "Enter 's <speed>', 'm <0-3>', or 'c <mode>': ");
        serial_write ;
        // spawn a thread to do the non-blocking serial read
        serial_read ;
        // convert input string to number
        sscanf(pt_serial_in_buffer, "%c %f", &cmd, &value);
        if (cmd == 's') {
             if (value >= 0) sweep_speed = value;
        } else if (cmd == 'm') {
             int motor = (int)value;
             if (motor >= 0 && motor <= 3) active_motor = motor;
        } else if (cmd == 'c') {
             control_mode = (int)value;
        } else if (cmd == 'r') {
             motor_reset((int)value);
        } else if (cmd == 't') {
             motor_move_1_2((int)value);
        } else if (cmd == 'w') {
             motor_walk();
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
