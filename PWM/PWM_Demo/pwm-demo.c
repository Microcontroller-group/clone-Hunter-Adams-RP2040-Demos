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
#define PWM_PIN1 4  // Physical pin 6
#define PWM_PIN2 5  // Physical pin 7
#define PWM_PIN3 6  // Physical pin 9
#define BUTTON_PIN 20 // External button pin

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

// Target angles for each motor
volatile int target_angle1 = 90;
volatile int target_angle2 = 90;
volatile int target_angle3 = 90;

// Offsets for each motor
volatile int offset_motor1 = 5;
volatile int offset_motor2 = -5;
volatile int offset_motor3 = 0;

// Moving range limits
volatile int min_angle1 = 10;
volatile int max_angle1 = 170;
volatile int min_angle2 = 10;
volatile int max_angle2 = 170;
volatile int min_angle3 = 60;
volatile int max_angle3 = 120;

// Helper macro for clamping
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

// Stop flag
volatile bool stop_motors = false;

// Test mode flags
volatile bool test_mode1 = false;
volatile bool test_mode2 = false;
volatile bool test_mode3 = false;

// Walk mode flag
volatile bool walk_mode = false;
volatile bool walkrepeat_mode = false;

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
            // Test mode logic for Motor 1
            if (test_mode1) {
                if (angle1 >= max_angle1 + offset_motor1) target_angle1 = min_angle1 + offset_motor1;
                else if (angle1 <= min_angle1 + offset_motor1) target_angle1 = max_angle1 + offset_motor1;
            }
            // Test mode logic for Motor 2
            if (test_mode2) {
                if (angle2 >= max_angle2 + offset_motor2) target_angle2 = min_angle2 + offset_motor2;
                else if (angle2 <= min_angle2 + offset_motor2) target_angle2 = max_angle2 + offset_motor2;
            }
            // Test mode logic for Motor 3
            if (test_mode3) {
                if (angle3 >= max_angle3 + offset_motor3) target_angle3 = min_angle3 + offset_motor3;
                else if (angle3 <= min_angle3 + offset_motor3) target_angle3 = max_angle3 + offset_motor3;
            }

            // Update Motor 1
            if (angle1 < target_angle1) angle1 += angle_step;
            else if (angle1 > target_angle1) angle1 -= angle_step;
            control1 = angle_to_duty_cycle(angle1);

            // Update Motor 2
            if (angle2 < target_angle2) angle2 += angle_step;
            else if (angle2 > target_angle2) angle2 -= angle_step;
            control2 = angle_to_duty_cycle(angle2);

            // Update Motor 3
            if (angle3 < target_angle3) angle3 += angle_step;
            else if (angle3 > target_angle3) angle3 -= angle_step;
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
    static char cmd[32];
    static float value;
    while(1) {
        sprintf(pt_serial_out_buffer, "Enter cmd (x, speed, set, set1/2/3, set12, set12invert, test1/2/3, offset1/2/3, print, walk, walkrepeat): ");
        serial_write ;
        // spawn a thread to do the non-blocking serial read
        serial_read ;
        // convert input string to number
        sscanf(pt_serial_in_buffer, "%s %f", cmd, &value);
        
        if (strcmp(cmd, "x") == 0) {
             stop_motors = true;
             test_mode1 = false;
             test_mode2 = false;
             test_mode3 = false;
             walk_mode = false;
             walkrepeat_mode = false;
        } else if (strcmp(cmd, "speed") == 0) {
             if (value > 0) motor_speed = value;
        } else if (strcmp(cmd, "set") == 0) {
             if (value >= 0 && value <= 180) {
                 target_angle1 = CLAMP((int)value, min_angle1, max_angle1) + offset_motor1;
                 target_angle2 = 180 - CLAMP((int)value, min_angle2, max_angle2) + offset_motor2;
                 target_angle3 = CLAMP((int)value, min_angle3, max_angle3) + offset_motor3;
                 stop_motors = false;
                 test_mode1 = false;
                 test_mode2 = false;
                 test_mode3 = false;
                 walk_mode = false;
                 walkrepeat_mode = false;
             }
        } else if (strcmp(cmd, "set1") == 0) {
             if (value >= 0 && value <= 180) {
                 target_angle1 = CLAMP((int)value, min_angle1, max_angle1) + offset_motor1;
                 stop_motors = false;
                 test_mode1 = false;
                 walk_mode = false;
                 walkrepeat_mode = false;
             }
        } else if (strcmp(cmd, "set2") == 0) {
             if (value >= 0 && value <= 180) {
                 target_angle2 = 180 - CLAMP((int)value, min_angle2, max_angle2) + offset_motor2;
                 stop_motors = false;
                 test_mode2 = false;
                 walk_mode = false;
                 walkrepeat_mode = false;
             }
        } else if (strcmp(cmd, "set3") == 0) {
             if (value >= 0 && value <= 180) {
                 target_angle3 = CLAMP((int)value, min_angle3, max_angle3) + offset_motor3;
                 stop_motors = false;
                 test_mode3 = false;
                 walk_mode = false;
                 walkrepeat_mode = false;
             }
        } else if (strcmp(cmd, "set12") == 0) {
             if (value >= 0 && value <= 180) {
                 target_angle1 = CLAMP((int)value, min_angle1, max_angle1) + offset_motor1;
                 target_angle2 = 180 - CLAMP((int)value, min_angle2, max_angle2) + offset_motor2;
                 stop_motors = false;
                 test_mode1 = false;
                 test_mode2 = false;
                 walk_mode = false;
                 walkrepeat_mode = false;
             }
        } else if (strcmp(cmd, "set12invert") == 0) {
             if (value >= 0 && value <= 180) {
                 target_angle1 = CLAMP((int)value, min_angle1, max_angle1) + offset_motor1;
                 target_angle2 = CLAMP((int)value, min_angle2, max_angle2) + offset_motor2;
                 stop_motors = false;
                 test_mode1 = false;
                 test_mode2 = false;
                 walk_mode = false;
                 walkrepeat_mode = false;
             }
        } else if (strcmp(cmd, "test1") == 0) {
             test_mode1 = true;
             stop_motors = false;
             walk_mode = false;
             walkrepeat_mode = false;
             // Kickstart movement
             if (angle1 >= max_angle1 + offset_motor1) target_angle1 = min_angle1 + offset_motor1;
             else target_angle1 = max_angle1 + offset_motor1;
        } else if (strcmp(cmd, "test2") == 0) {
             test_mode2 = true;
             stop_motors = false;
             walk_mode = false;
             walkrepeat_mode = false;
             // Kickstart movement
             if (angle2 >= max_angle2 + offset_motor2) target_angle2 = min_angle2 + offset_motor2;
             else target_angle2 = max_angle2 + offset_motor2;
        } else if (strcmp(cmd, "test3") == 0) {
             test_mode3 = true;
             stop_motors = false;
             walk_mode = false;
             walkrepeat_mode = false;
             // Kickstart movement
             if (angle3 >= max_angle3 + offset_motor3) target_angle3 = min_angle3 + offset_motor3;
             else target_angle3 = max_angle3 + offset_motor3;
        } else if (strcmp(cmd, "walk") == 0) {
             walk_mode = true;
             stop_motors = false;
             test_mode1 = false;
             test_mode2 = false;
             test_mode3 = false;
             walkrepeat_mode = false;
        } else if (strcmp(cmd, "walkrepeat") == 0) {
             walkrepeat_mode = true;
             stop_motors = false;
             test_mode1 = false;
             test_mode2 = false;
             test_mode3 = false;
             walk_mode = false;
        } else if (strcmp(cmd, "offset1") == 0) {
             offset_motor1 = (int)value;
        } else if (strcmp(cmd, "offset2") == 0) {
             offset_motor2 = (int)value;
        } else if (strcmp(cmd, "offset3") == 0) {
             offset_motor3 = (int)value;
        } else if (strcmp(cmd, "print") == 0) {
             sprintf(pt_serial_out_buffer, "Offsets: M1=%d, M2=%d, M3=%d\n", offset_motor1, offset_motor2, offset_motor3);
             serial_write;
        }
    }
    PT_END(pt) ;
}

// Walk mode thread
static PT_THREAD (protothread_walk(struct pt *pt)) {
    PT_BEGIN(pt);
    while(1) {
        PT_WAIT_UNTIL(pt, walk_mode);
        
        // Step 1: set3 60
        motor_speed = 60.0;
        target_angle3 = CLAMP(60, min_angle3, max_angle3) + offset_motor3;
        PT_WAIT_UNTIL(pt, angle3 == target_angle3);
        PT_YIELD_usec(200000);

        // Step 2: set12 55
        motor_speed = 200.0;
        target_angle1 = CLAMP(55, min_angle1, max_angle1) + offset_motor1;
        target_angle2 = 180 - CLAMP(55, min_angle2, max_angle2) + offset_motor2;
        PT_WAIT_UNTIL(pt, angle1 == target_angle1 && angle2 == target_angle2);
        PT_YIELD_usec(1200000);

        // Step 3: set3 120
        motor_speed = 60.0;
        target_angle3 = CLAMP(120, min_angle3, max_angle3) + offset_motor3;
        // PT_WAIT_UNTIL(pt, angle3 == target_angle3);
        PT_YIELD_usec(500000);

        // Step 4: set12 90
        target_angle1 = CLAMP(90, min_angle1, max_angle1) + offset_motor1;
        target_angle2 = 180 - CLAMP(90, min_angle2, max_angle2) + offset_motor2;
        PT_WAIT_UNTIL(pt, angle1 == target_angle1 && angle2 == target_angle2);
        // PT_YIELD_usec(200000);

        // Step 5: set3 90
        motor_speed = 60.0;
        target_angle3 = CLAMP(90, min_angle3, max_angle3) + offset_motor3;
        PT_WAIT_UNTIL(pt, angle3 == target_angle3);
        // PT_YIELD_usec(200000);

        motor_speed = 60.0;
        walk_mode = false;
    }
    PT_END(pt);
}

// Calibration thread
static PT_THREAD (protothread_calibrate(struct pt *pt)) {
    PT_BEGIN(pt);
    
    // Small delay to let everything init
    PT_YIELD_usec(1000000);

    // Step 1: set3 60
    target_angle3 = CLAMP(60, min_angle3, max_angle3) + offset_motor3;
    PT_WAIT_UNTIL(pt, angle3 == target_angle3);
    PT_YIELD_usec(200000);

    // Step 2: set3 120
    target_angle3 = CLAMP(120, min_angle3, max_angle3) + offset_motor3;
    PT_WAIT_UNTIL(pt, angle3 == target_angle3);
    PT_YIELD_usec(200000);

    // Step 3: set1 170
    target_angle1 = CLAMP(170, min_angle1, max_angle1) + offset_motor1;
    PT_WAIT_UNTIL(pt, angle1 == target_angle1);
    PT_YIELD_usec(200000);

    // Step 4: set1 10
    target_angle1 = CLAMP(10, min_angle1, max_angle1) + offset_motor1;
    PT_WAIT_UNTIL(pt, angle1 == target_angle1);
    PT_YIELD_usec(200000);

    // Step 5: set1 90
    target_angle1 = CLAMP(90, min_angle1, max_angle1) + offset_motor1;
    PT_WAIT_UNTIL(pt, angle1 == target_angle1);
    PT_YIELD_usec(200000);

    // Step 6: set2 170
    target_angle2 = 180 - CLAMP(170, min_angle2, max_angle2) + offset_motor2;
    PT_WAIT_UNTIL(pt, angle2 == target_angle2);
    PT_YIELD_usec(200000);

    // Step 7: set2 10
    target_angle2 = 180 - CLAMP(10, min_angle2, max_angle2) + offset_motor2;
    PT_WAIT_UNTIL(pt, angle2 == target_angle2);
    PT_YIELD_usec(200000);

    // Step 8: set2 90
    target_angle2 = 180 - CLAMP(90, min_angle2, max_angle2) + offset_motor2;
    PT_WAIT_UNTIL(pt, angle2 == target_angle2);
    PT_YIELD_usec(200000);

    // Step 9: set 90
    target_angle1 = CLAMP(90, min_angle1, max_angle1) + offset_motor1;
    target_angle2 = 180 - CLAMP(90, min_angle2, max_angle2) + offset_motor2;
    target_angle3 = CLAMP(90, min_angle3, max_angle3) + offset_motor3;
    PT_WAIT_UNTIL(pt, angle1 == target_angle1 && angle2 == target_angle2 && angle3 == target_angle3);
    
    // Done, spin forever
    while(1) {
        PT_YIELD(pt);
    }

    PT_END(pt);
}

// Walkrepeat mode thread
static PT_THREAD (protothread_walkrepeat(struct pt *pt)) {
    PT_BEGIN(pt);
    while(1) {
        PT_WAIT_UNTIL(pt, walkrepeat_mode);
        
        walk_mode = true;
        // Wait for walk_mode to be cleared by the walk thread
        PT_WAIT_UNTIL(pt, !walk_mode);
    }
    PT_END(pt);
}

// Switch thread
static PT_THREAD (protothread_switch(struct pt *pt)) {
    PT_BEGIN(pt);
    static bool switch_state;
    while(1) {
        // Read switch state (Active High)
        switch_state = gpio_get(BUTTON_PIN);

        if (switch_state && !walkrepeat_mode) {
            // Switch is ON, but mode is OFF -> Turn ON
            walkrepeat_mode = true;
            stop_motors = false;
            test_mode1 = false;
            test_mode2 = false;
            test_mode3 = false;
            walk_mode = false; 
        } else if (!switch_state && walkrepeat_mode) {
            // Switch is OFF, but mode is ON -> Turn OFF
            walkrepeat_mode = false;
        }

        PT_YIELD_usec(100000);
    }
    PT_END(pt);
}

// LED blink thread
static PT_THREAD (protothread_blink(struct pt *pt))
{
    PT_BEGIN(pt);
    while(1) {
        gpio_put(PICO_DEFAULT_LED_PIN, !gpio_get(PICO_DEFAULT_LED_PIN));
        PT_YIELD_usec(500000);
    }
    PT_END(pt);
}

int main() {

    // Initialize stdio
    stdio_init_all();

    // Initialize LED
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    // Initialize Button
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_down(BUTTON_PIN);

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
    pt_add_thread(protothread_blink) ;
    pt_add_thread(protothread_calibrate) ;
    pt_add_thread(protothread_walk) ;
    pt_add_thread(protothread_walkrepeat) ;
    pt_add_thread(protothread_switch) ;
    pt_schedule_start ;

}
