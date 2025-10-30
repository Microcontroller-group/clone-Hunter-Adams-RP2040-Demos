/**
 * V. Hunter Adams (vha3@cornell.edu)
 * 
 * This demonstration utilizes the MPU6050.
 * It gathers raw accelerometer/gyro measurements, scales
 * them, and plots them to the VGA display. The top plot
 * shows gyro measurements, bottom plot shows accelerometer
 * measurements.
 * 
 * HARDWARE CONNECTIONS
 *  - GPIO 16 ---> VGA Hsync
 *  - GPIO 17 ---> VGA Vsync
 *  - GPIO 18 ---> 470 ohm resistor ---> VGA Green
 *  - GPIO 19 ---> 330 ohm resistor ---> VGA Green
 *  - GPIO 20 ---> 330 ohm resistor ---> VGA Blue
 *  - GPIO 21 ---> 330 ohm resistor ---> VGA Red
 *  - RP2040 GND ---> VGA GND
 *  - GPIO 8 ---> MPU6050 SDA
 *  - GPIO 9 ---> MPU6050 SCL
 *  - 3.3v ---> MPU6050 VCC
 *  - RP2040 GND ---> MPU6050 GND
 */


// Include standard libraries
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
// Include PICO libraries
#include "pico/stdlib.h"
#include "pico/multicore.h"
// Include hardware libraries
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "hardware/i2c.h"
#include "hardware/clocks.h"
// Include custom libraries
#include "vga16_graphics_v2.h"
#include "mpu6050.h"
#include "pt_cornell_rp2040_v1_4.h"

int TEST = 0;

// Button and sequence control
#define BUTTON_PIN 2
volatile int button_pressed = 0;
volatile int button_held = 0;
volatile int sequence_active = 0;
volatile uint32_t sequence_start_time = 0;
volatile uint32_t sequence_timer = 0;
volatile int motor_disabled = 0;


// Arrays in which raw measurements will be stored
fix15 acceleration[3], gyro[3];
fix15 accel_angle;
fix15 gyro_angle_delta;
fix15 angular_velocity;
fix15 filtered_ay, filtered_az;
fix15 complementary_angle;

// PID control parameters
fix15 Kp = float2fix15(50.0);
fix15 Ki = float2fix15(50.0);
fix15 Kd = float2fix15(15.0);
fix15 target_angle = int2fix15(0);
fix15 current_angle = int2fix15(0);
fix15 error_angle;
fix15 error_sum = int2fix15(0);
volatile int parameter_update = 1;

// character array
char screentext[40];

// draw speed
int threshold = 10 ;

// Some macros for max/min/abs
#define min(a,b) ((a<b) ? a:b)
#define max(a,b) ((a<b) ? b:a)
#define abs(a) ((a>0) ? a:-a)

// semaphore
static struct pt_sem vga_semaphore ;

// Some paramters for PWM
#define WRAPVAL 5000
#define CLKDIV  25.0
uint slice_num ;

// PWM duty cycle
volatile int control = 0;  // Duty cycle (0-5000)
volatile int old_control = 0;
int control_filtered = 0 ;

// Interrupt service routine
void on_pwm_wrap() {

    // Clear the interrupt flag that brought us here
    pwm_clear_irq(pwm_gpio_to_slice_num(5));

    // Read the IMU
    // NOTE! This is in 15.16 fixed point. Accel in g's, gyro in deg/s
    // If you want these values in floating point, call fix2float15() on
    // the raw measurements.
    mpu6050_read_raw(acceleration, gyro);
    filtered_ay = filtered_ay + ( (acceleration[1] - filtered_ay) >> 4 );
    filtered_az = filtered_az + ( (acceleration[2] - filtered_az) >> 4 );
    accel_angle = multfix15(float2fix15(atan2(filtered_az, -filtered_ay)), oneeightyoverpi);
    gyro_angle_delta = multfix15(gyro[0], zeropt001);
    angular_velocity = gyro[0];
    complementary_angle = multfix15(complementary_angle + gyro_angle_delta, zeropt999) + multfix15(accel_angle, zeropt001);

    // Handle sequence timing (1kHz ISR = 1ms per call)
    if (sequence_active) {
        sequence_timer++;

        // Update target angle based on sequence time
        if (sequence_timer < 5000) {
            // 0-5 seconds: horizontal (90 degrees)
            if ( target_angle != int2fix15(90) ) parameter_update = 1;
            target_angle = int2fix15(90);
        } else if (sequence_timer < 10000) {
            // 5-10 seconds: 30 degrees above horizontal (120 degrees)
            if ( target_angle != int2fix15(120) ) parameter_update = 1;
            target_angle = int2fix15(120);
        } else if (sequence_timer < 15000) {
            // 10-15 seconds: 30 degrees below horizontal (60 degrees)
            if ( target_angle != int2fix15(60) ) parameter_update = 1;
            target_angle = int2fix15(60);
        } else if (sequence_timer < 20000) {
            // 15-20 seconds: back to horizontal (90 degrees)
            if ( target_angle != int2fix15(90) ) parameter_update = 1;
            target_angle = int2fix15(90);
        } else {
            // Sequence complete
            sequence_active = 0;
            sequence_timer = 0;
        }
    }

    // PID control
    current_angle = complementary_angle;
    error_angle = current_angle - target_angle;
    error_sum = error_sum + multfix15(error_angle, zeropt001);

    if ( error_sum > int2fix15(50) ) error_sum = int2fix15(50);
    else if ( error_sum < int2fix15(-50) ) error_sum = int2fix15(-50);

    if ( motor_disabled ) {
        control = 0;
    } else {
        control = fix2int15( - multfix15(Kp, error_angle) - multfix15(Ki, error_sum) - multfix15(Kd, angular_velocity) );
        if (control > 2300) control = 2300;
        else if (control < 0) control = 0;
    }

    // Update duty cycle
    if (control != old_control) {
        pwm_set_chan_level(slice_num, PWM_CHAN_B, control);
        pwm_set_chan_level(slice_num, PWM_CHAN_A, control);
        old_control = control;
    }

    control_filtered = control_filtered + ( (control - control_filtered) >> 4 );

    // Signal VGA to draw
    PT_SEM_SIGNAL(pt, &vga_semaphore);
}

// Button interrupt handler
void button_irq_handler(uint gpio, uint32_t events) {
    if (gpio == BUTTON_PIN) {
        // Button pressed (falling edge) - disable motor, arm hangs down
        if (events & GPIO_IRQ_EDGE_FALL) {
            motor_disabled = 1;
            sequence_active = 0;
            sequence_timer = 0;
        }
        // Button released (rising edge) - enable motor and start sequence
        else if (events & GPIO_IRQ_EDGE_RISE) {
            motor_disabled = 0;
            sequence_active = 1;
            sequence_timer = 0;
            // First target: horizontal (90 degrees)
            parameter_update = 1;
            target_angle = int2fix15(90);
            error_sum = int2fix15(0);
        }
    }
}

// Thread that draws to VGA display
static PT_THREAD (protothread_vga(struct pt *pt))
{
    // Indicate start of thread
    PT_BEGIN(pt) ;

    // We will start drawing at column 81
    static int xcoord = 81 ;
    
    // Rescale the measurements for display
    static float OldRange = 500. ; // (+/- 250)
    static float NewRange = 150. ; // (looks nice on VGA)
    static float OldMin = -250. ;
    static float OldMax = 250. ;

    // Control rate of drawing
    static int throttle ;

    // Draw the static aspects of the display
    setTextSize(1) ;
    setTextColor(WHITE);

    // Draw bottom plot
    drawHLine(75, 430, 5, CYAN) ;
    drawHLine(75, 355, 5, CYAN) ;
    drawHLine(75, 280, 5, CYAN) ;
    drawVLine(80, 280, 150, CYAN) ;
    // sprintf(screentext, "0") ;
    sprintf(screentext, "0.5") ;
    setCursor(50, 350) ;
    writeString(screentext) ;
    // sprintf(screentext, "+2") ;
    sprintf(screentext, "1.0") ;
    setCursor(50, 280) ;
    writeString(screentext) ;
    // sprintf(screentext, "-2") ;
    sprintf(screentext, "0.0") ;
    setCursor(50, 425) ;
    writeString(screentext) ;

    // Draw top plot (Complementary filter angle)
    drawHLine(75, 230, 5, CYAN) ;
    drawHLine(75, 155, 5, CYAN) ;
    drawHLine(75, 80, 5, CYAN) ;
    drawVLine(80, 80, 150, CYAN) ;
    // sprintf(screentext, "0") ;
    sprintf(screentext, "90") ;
    setCursor(50, 150) ;
    writeString(screentext) ;
    // sprintf(screentext, "+250") ;
    sprintf(screentext, "180") ;
    setCursor(45, 75) ;
    writeString(screentext) ;
    // sprintf(screentext, "-250") ;
    sprintf(screentext, "0") ;
    setCursor(45, 225) ;
    writeString(screentext) ;

    while (true) {
        // Wait on semaphore
        PT_SEM_WAIT(pt, &vga_semaphore);
        // Increment drawspeed controller
        throttle += 1 ;
        // If the controller has exceeded a threshold, draw
        if (throttle >= threshold) { 
            // Zero drawspeed controller
            throttle = 0 ;

            // Erase a column
            drawVLine(xcoord, 80, 480, BLACK) ;

            // Draw bottom plot (PWM duty cycle)
            // drawPixel(xcoord, 430 - (int)(NewRange*((float)((fix2float15(acceleration[0])*120.0)-OldMin)/OldRange)), WHITE) ;
            // drawPixel(xcoord, 430 - (int)(NewRange*((float)((fix2float15(acceleration[1])*120.0)-OldMin)/OldRange)), RED) ;
            // drawPixel(xcoord, 430 - (int)(NewRange*((float)((fix2float15(acceleration[2])*120.0)-OldMin)/OldRange)), GREEN) ;
            drawPixel(xcoord, 430 - (int)(NewRange*((float)(control_filtered*500/5000)/OldRange)), YELLOW) ;

            // Draw top plot (Complementary filter angle)
            // drawPixel(xcoord, 230 - (int)(NewRange*((float)((fix2float15(gyro[0]))-OldMin)/OldRange)), WHITE) ;
            // drawPixel(xcoord, 230 - (int)(NewRange*((float)((fix2float15(gyro[1]))-OldMin)/OldRange)), RED) ;
            // drawPixel(xcoord, 230 - (int)(NewRange*((float)((fix2float15(gyro[2]))-OldMin)/OldRange)), GREEN) ;
            drawPixel(xcoord, 230 - (int)(NewRange*((float)(fix2float15(complementary_angle)*500.0/180.0)/OldRange)), YELLOW) ;
            // Draw target angle line
            drawPixel(xcoord, 230 - (int)(NewRange*((float)(fix2float15(target_angle)*500.0/180.0)/OldRange)), CYAN) ;

            // Update horizontal cursor
            if (xcoord < 609) {
                xcoord += 1 ;
            }
            else {
                xcoord = 81 ;
            }
        }

        // Top graph title
        sprintf(screentext, "Beam angle (degrees)") ;
        setCursor(240, 55) ;
        writeString(screentext);
        // Bottom graph title
        sprintf(screentext, "PWM duty cycle (0.0-1.0)") ;
        setCursor(240, 260) ;
        writeString(screentext);

        // Current PID parameters
        if ( parameter_update ) {
            parameter_update = 0 ;
            // Delete previous text
            fillRect(0, 0, 180, 70, BLACK) ;
            // Write new text
            sprintf(screentext, "Target beam angle: %d", fix2int15(target_angle)) ;
            setCursor(20, 10) ;
            writeString(screentext);
            sprintf(screentext, "Kp: %.2f", fix2float15(Kp)) ;
            setCursor(20, 25) ;
            writeString(screentext);
            sprintf(screentext, "Ki: %.2f", fix2float15(Ki)) ;
            setCursor(20, 40) ;
            writeString(screentext);
            sprintf(screentext, "Kd: %.2f", fix2float15(Kd)) ;
            setCursor(20, 55) ;
            writeString(screentext);
        }

    }
    // Indicate end of thread
    PT_END(pt);
}

// User input thread. User can change draw speed
static PT_THREAD (protothread_serial(struct pt *pt))
{
    PT_BEGIN(pt) ;
    static char classifier ;
    static int test_in ;
    static float float_in ;
    while(1) {
        sprintf(pt_serial_out_buffer, "Set desired beam angle: enter \"a\"\r\n");
        serial_write;
        sprintf(pt_serial_out_buffer, "Set proportional gain: enter \"p\"\r\n");
        serial_write;
        sprintf(pt_serial_out_buffer, "Set integral gain: enter \"i\"\r\n");
        serial_write;
        sprintf(pt_serial_out_buffer, "Set derivative gain: enter \"d\"\r\n");
        serial_write;
        sprintf(pt_serial_out_buffer, "Reset PID parameters: enter \"r\"\r\n");
        serial_write;
        sprintf(pt_serial_out_buffer, "Print current PID parameters: enter \"c\"\r\n");
        serial_write;
        sprintf(pt_serial_out_buffer, "Set threshold: enter \"t\"\r\n");
        serial_write;
        serial_read;
        sscanf(pt_serial_in_buffer,"%c", &classifier);
        if (classifier=='a') {
            sprintf(pt_serial_out_buffer, "Input target angle (0-180): ");
            serial_write;
            serial_read;
            sscanf(pt_serial_in_buffer,"%d", &test_in);
            if (test_in > 180) continue;
            else if (test_in < 0) continue;
            else target_angle = int2fix15(test_in);
            parameter_update = 1;
        } else if (classifier=='p') {
            sprintf(pt_serial_out_buffer, "Input Kp (float): ");
            serial_write;
            serial_read;
            sscanf(pt_serial_in_buffer,"%f", &float_in);
            if (float_in < 0) continue;
            else Kp = float2fix15(float_in);
            parameter_update = 1;
        } else if (classifier=='i') {
            sprintf(pt_serial_out_buffer, "Input Ki (float): ");
            serial_write;
            serial_read;
            sscanf(pt_serial_in_buffer,"%f", &float_in);
            if (float_in < 0) continue;
            else Ki = float2fix15(float_in);
            parameter_update = 1;
        } else if (classifier=='d') {
            sprintf(pt_serial_out_buffer, "Input Kd (float): ");
            serial_write;
            serial_read;
            sscanf(pt_serial_in_buffer,"%f", &float_in);
            if (float_in < 0) continue;
            else Kd = float2fix15(float_in);
            parameter_update = 1;
        } else if (classifier=='r') {
            Kp = float2fix15(50.0);
            Ki = float2fix15(50.0);
            Kd = float2fix15(15.0);
            parameter_update = 1;
        } else if (classifier=='c') {
            sprintf(pt_serial_out_buffer, "Current PID parameters:\r\n");
            serial_write;
            sprintf(pt_serial_out_buffer, "Kp: %.2f\r\n", fix2float15(Kp));
            serial_write;
            sprintf(pt_serial_out_buffer, "Ki: %.2f\r\n", fix2float15(Ki));
            serial_write;
            sprintf(pt_serial_out_buffer, "Kd: %.2f\r\n", fix2float15(Kd));
            serial_write;
        } else if ( classifier=='t') {
            sprintf(pt_serial_out_buffer, "Input threshold (1-100): ");
            serial_write;
            serial_read;
            sscanf(pt_serial_in_buffer,"%d", &test_in);
            if (test_in < 1) continue;
            else if (test_in > 100) continue;
            else threshold = test_in ;
        } else {
            sprintf(pt_serial_out_buffer, "Invalid command\r\n");
            serial_write;
        }

    }
    PT_END(pt) ;
}

// Entry point for core 1
void core1_entry() {
    pt_add_thread(protothread_vga) ;
    pt_schedule_start ;
}

int main() {

    // Overclock
    set_sys_clock_khz(150000, true) ;

    // Initialize stdio
    stdio_init_all();

    // Initialize VGA
    initVGA() ;

    ////////////////////////////////////////////////////////////////////////
    ///////////////////////// BUTTON CONFIGURATION /////////////////////////
    ////////////////////////////////////////////////////////////////////////
    // Initialize button GPIO
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    // Set up button interrupt on both falling edge (press) and rising edge (release)
    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &button_irq_handler);

    ////////////////////////////////////////////////////////////////////////
    ///////////////////////// I2C CONFIGURATION ////////////////////////////
    i2c_init(I2C_CHAN, I2C_BAUD_RATE) ;
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C) ;
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C) ;

    // Pullup resistors on breakout board, don't need to turn on internals
    // gpio_pull_up(SDA_PIN) ;
    // gpio_pull_up(SCL_PIN) ;

    // MPU6050 initialization
    mpu6050_reset();
    mpu6050_read_raw(acceleration, gyro);

    ////////////////////////////////////////////////////////////////////////
    ///////////////////////// PWM CONFIGURATION ////////////////////////////
    ////////////////////////////////////////////////////////////////////////
    // Tell GPIO's 4,5 that they allocated to the PWM
    gpio_set_function(5, GPIO_FUNC_PWM);
    gpio_set_function(4, GPIO_FUNC_PWM);

    // Find out which PWM slice is connected to GPIO 5 (it's slice 2, same for 4)
    slice_num = pwm_gpio_to_slice_num(5);

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
    pwm_set_chan_level(slice_num, PWM_CHAN_B, 0);
    pwm_set_chan_level(slice_num, PWM_CHAN_A, 0);

    // Start the channel
    pwm_set_mask_enabled((1u << slice_num));


    ////////////////////////////////////////////////////////////////////////
    ///////////////////////////// ROCK AND ROLL ////////////////////////////
    ////////////////////////////////////////////////////////////////////////
    // start core 1 
    multicore_reset_core1();
    multicore_launch_core1(core1_entry);

    // start core 0
    pt_add_thread(protothread_serial) ;
    pt_schedule_start ;

}
