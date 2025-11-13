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

// Test flag for debugging purposes
int TEST = 0;

////////////////////////////////////////////////////////////////////////
///////////////////////// BUTTON & SEQUENCE CONTROL ///////////////////
////////////////////////////////////////////////////////////////////////
// GPIO pin connected to the control button
#define BUTTON_PIN 2

// Button state flags (volatile because they're modified in ISR)
volatile int button_pressed = 0;   // Flag indicating button is currently pressed
volatile int button_held = 0;      // Flag indicating button is being held down
volatile int sequence_active = 0;  // Flag indicating automated sequence is running
volatile uint32_t sequence_start_time = 0; // Timestamp when sequence started
volatile uint32_t sequence_timer = 0;      // Millisecond counter for sequence timing
volatile int motor_disabled = 0;   // Flag to disable motor control (safety feature)


////////////////////////////////////////////////////////////////////////
///////////////////////// IMU SENSOR DATA /////////////////////////////
////////////////////////////////////////////////////////////////////////
// Raw sensor measurements from MPU6050 (in 15.16 fixed-point format)
// All values use fix15 format: acceleration in g's, gyro in deg/s
fix15 acceleration[3];  // [x, y, z] accelerometer readings in g's
fix15 gyro[3];          // [x, y, z] gyroscope readings in degrees/second

// Processed IMU data for angle calculation
fix15 accel_angle;         // Angle calculated from accelerometer data
fix15 gyro_angle_delta;    // Change in angle from gyroscope integration
fix15 angular_velocity;    // Current angular velocity (gyro[0])
fix15 filtered_ay;         // Low-pass filtered Y-axis acceleration
fix15 filtered_az;         // Low-pass filtered Z-axis acceleration
fix15 complementary_angle; // Final angle estimate using complementary filter

////////////////////////////////////////////////////////////////////////
///////////////////////// PID CONTROL PARAMETERS //////////////////////
////////////////////////////////////////////////////////////////////////
// PID controller gains (tunable for system response)
fix15 Kp = float2fix15(50.0);  // Proportional gain: responds to current error
fix15 Ki = float2fix15(50.0);  // Integral gain: eliminates steady-state error
fix15 Kd = float2fix15(15.0);  // Derivative gain: dampens oscillations

// Angle setpoint and tracking
fix15 target_angle = int2fix15(0);   // Desired beam angle (0-180 degrees)
fix15 current_angle = int2fix15(0);  // Current beam angle from sensor
fix15 error_angle;                    // Difference between target and current
fix15 error_sum = int2fix15(0);      // Accumulated error for integral term

// Flag to trigger VGA display update when parameters change
volatile int parameter_update = 1;

////////////////////////////////////////////////////////////////////////
///////////////////////// VGA DISPLAY SETTINGS ////////////////////////
////////////////////////////////////////////////////////////////////////
// Buffer for formatting text to display on VGA screen
char screentext[40];

// Drawing throttle: higher values = slower screen updates (reduces flicker)
int threshold = 10;

// Utility macros for common operations
#define min(a,b) ((a<b) ? a:b)
#define max(a,b) ((a<b) ? b:a)
#define abs(a) ((a>0) ? a:-a)

// Semaphore to synchronize VGA drawing with IMU interrupt
static struct pt_sem vga_semaphore;

////////////////////////////////////////////////////////////////////////
///////////////////////// PWM MOTOR CONTROL ////////////////////////////
////////////////////////////////////////////////////////////////////////
// PWM configuration constants
#define WRAPVAL 5000   // PWM counter wrap value (determines period)
#define CLKDIV  25.0   // Clock divider for PWM frequency
uint slice_num;        // PWM slice number (determined at runtime)

// PWM duty cycle control (0-5000 maps to 0-100% duty cycle)
volatile int control = 0;       // Current duty cycle command from PID
volatile int old_control = 0;   // Previous duty cycle (to detect changes)
int control_filtered = 0;       // Low-pass filtered duty cycle for display

////////////////////////////////////////////////////////////////////////
///////////////////////// PWM WRAP ISR /////////////////////////////////
////////////////////////////////////////////////////////////////////////
/**
 * PWM Wrap Interrupt Service Routine
 * 
 * This ISR is triggered at 1kHz by the PWM counter wrap event.
 * It performs three main tasks:
 * 1. Read and process IMU sensor data
 * 2. Calculate beam angle using complementary filter
 * 3. Execute PID control algorithm to adjust motor duty cycle
 */
void on_pwm_wrap() {

    // Clear the interrupt flag that brought us here
    pwm_clear_irq(pwm_gpio_to_slice_num(5));

    ////////////////////////////////////////////////////////////////////////
    ///////////////////////// IMU DATA ACQUISITION /////////////////////////
    ////////////////////////////////////////////////////////////////////////
    // Read raw accelerometer and gyroscope data from MPU6050
    // NOTE! This is in 15.16 fixed point format:
    //   - Acceleration values are in g's (gravitational units)
    //   - Gyroscope values are in degrees/second
    // Use fix2float15() to convert to floating point if needed
    mpu6050_read_raw(acceleration, gyro);
    
    // Apply low-pass filter to Y and Z accelerometer axes
    // This reduces noise in angle calculation
    // Filter equation: filtered = filtered + (new - filtered) / 16
    filtered_ay = filtered_ay + ( (acceleration[1] - filtered_ay) >> 4 );
    filtered_az = filtered_az + ( (acceleration[2] - filtered_az) >> 4 );
    
    ////////////////////////////////////////////////////////////////////////
    ///////////////////////// COMPLEMENTARY FILTER /////////////////////////
    ////////////////////////////////////////////////////////////////////////
    // Calculate angle from accelerometer using atan2
    // atan2(z, -y) gives tilt angle in radians, converted to degrees
    accel_angle = multfix15(float2fix15(atan2(filtered_az, -filtered_ay)), oneeightyoverpi);
    
    // Integrate gyroscope to get change in angle
    // Multiply by 0.001 to convert from deg/s to deg (1ms sample period)
    gyro_angle_delta = multfix15(gyro[0], zeropt001);
    
    // Store angular velocity for derivative term of PID controller
    angular_velocity = gyro[0];
    
    // Complementary filter combines gyro and accelerometer:
    // angle = 0.999 * (angle + gyro_delta) + 0.001 * accel_angle
    // This trusts gyro for short-term changes, accel for long-term stability
    complementary_angle = multfix15(complementary_angle + gyro_angle_delta, zeropt999) + multfix15(accel_angle, zeropt001);

    ////////////////////////////////////////////////////////////////////////
    ///////////////////////// AUTOMATED SEQUENCE ///////////////////////////
    ////////////////////////////////////////////////////////////////////////
    // Handle automated test sequence (runs when button is released)
    // ISR runs at 1kHz, so sequence_timer increments every millisecond
    if (sequence_active) {
        sequence_timer++;

        // Update target angle based on elapsed time in the sequence
        // This creates a predetermined sequence of beam positions
        if (sequence_timer < 5000) {
            // Phase 1 (0-5 seconds): Hold beam horizontal at 90 degrees
            if ( target_angle != int2fix15(90) ) parameter_update = 1;
            target_angle = int2fix15(90);
        } else if (sequence_timer < 10000) {
            // Phase 2 (5-10 seconds): Tilt 30° above horizontal (120 degrees)
            if ( target_angle != int2fix15(120) ) parameter_update = 1;
            target_angle = int2fix15(120);
        } else if (sequence_timer < 15000) {
            // Phase 3 (10-15 seconds): Tilt 30° below horizontal (60 degrees)
            if ( target_angle != int2fix15(60) ) parameter_update = 1;
            target_angle = int2fix15(60);
        } else if (sequence_timer < 20000) {
            // Phase 4 (15-20 seconds): Return to horizontal (90 degrees)
            if ( target_angle != int2fix15(90) ) parameter_update = 1;
            target_angle = int2fix15(90);
        } else {
            // Sequence complete after 20 seconds - stop and reset
            sequence_active = 0;
            sequence_timer = 0;
        }
    }

    ////////////////////////////////////////////////////////////////////////
    ///////////////////////// PID CONTROL ALGORITHM ////////////////////////
    ////////////////////////////////////////////////////////////////////////
    // Update current angle from complementary filter
    current_angle = complementary_angle;
    
    // Calculate error (positive = beam above target)
    error_angle = current_angle - target_angle;
    
    // Accumulate error for integral term (multiply by 0.001 for 1ms timestep)
    error_sum = error_sum + multfix15(error_angle, zeropt001);

    // Anti-windup: Limit integral term to prevent saturation
    // Clamping prevents integral from growing too large during sustained errors
    if ( error_sum > int2fix15(50) ) error_sum = int2fix15(50);
    else if ( error_sum < int2fix15(-50) ) error_sum = int2fix15(-50);

    // Calculate PID control output
    if ( motor_disabled ) {
        // Safety mode: disable motor when button is pressed
        control = 0;
    } else {
        // Standard PID control equation:
        // u(t) = -Kp*e(t) - Ki*∫e(t)dt - Kd*de/dt
        // Negative signs because positive error should reduce duty cycle
        control = fix2int15( - multfix15(Kp, error_angle) - multfix15(Ki, error_sum) - multfix15(Kd, angular_velocity) );
        
        // Saturate output to safe operating range (0 to 46% duty cycle)
        if (control > 2300) control = 2300;
        else if (control < 0) control = 0;
    }

    // Update PWM duty cycle only if it has changed (reduces overhead)
    if (control != old_control) {
        pwm_set_chan_level(slice_num, PWM_CHAN_B, control);
        pwm_set_chan_level(slice_num, PWM_CHAN_A, control);
        old_control = control;
    }

    // Apply low-pass filter to control signal for smooth VGA display
    // filtered = filtered + (new - filtered) / 16
    control_filtered = control_filtered + ( (control - control_filtered) >> 4 );

    // Signal VGA thread that new data is ready to be displayed
    PT_SEM_SIGNAL(pt, &vga_semaphore);
}

////////////////////////////////////////////////////////////////////////
///////////////////////// BUTTON ISR ///////////////////////////////////
////////////////////////////////////////////////////////////////////////
/**
 * Button Interrupt Handler
 * 
 * Responds to button press/release events to control system behavior:
 * - Press (falling edge): Disable motor for safety (beam hangs freely)
 * - Release (rising edge): Re-enable motor and start automated test sequence
 */
void button_irq_handler(uint gpio, uint32_t events) {
    if (gpio == BUTTON_PIN) {
        // Button pressed (falling edge)
        // Immediately disable motor for safety - beam will hang down freely
        if (events & GPIO_IRQ_EDGE_FALL) {
            motor_disabled = 1;      // Turn off motor PWM output
            sequence_active = 0;     // Stop any running automated sequence
            sequence_timer = 0;      // Reset sequence timer
        }
        // Button released (rising edge)
        // Re-enable motor and initiate automated test sequence
        else if (events & GPIO_IRQ_EDGE_RISE) {
            motor_disabled = 0;          // Allow motor control
            sequence_active = 1;         // Start automated sequence
            sequence_timer = 0;          // Begin timing from zero
            parameter_update = 1;        // Trigger VGA display update
            target_angle = int2fix15(90); // First target: horizontal (90°)
            error_sum = int2fix15(0);    // Reset integral term for clean start
        }
    }
}

////////////////////////////////////////////////////////////////////////
///////////////////////// VGA DISPLAY THREAD ///////////////////////////
////////////////////////////////////////////////////////////////////////
/**
 * VGA Display Protothread
 * 
 * Continuously updates the VGA display with two scrolling plots:
 * - Top plot: Beam angle (0-180 degrees) vs time
 * - Bottom plot: PWM duty cycle (0.0-1.0) vs time
 * 
 * The display updates are synchronized with IMU readings via semaphore.
 */
static PT_THREAD (protothread_vga(struct pt *pt))
{
    // Indicate start of thread
    PT_BEGIN(pt) ;

    // Horizontal position where we will draw the next pixel
    static int xcoord = 81 ;
    
    // Scaling parameters to map sensor values to screen coordinates
    // Maps a range of -250 to +250 onto 150 pixels for display
    static float OldRange = 500. ; // Input range span
    static float NewRange = 150. ; // Output pixel span
    static float OldMin = -250. ;  // Minimum input value
    static float OldMax = 250. ;   // Maximum input value

    // Throttle counter to control drawing speed (prevents excessive flicker)
    static int throttle ;

    ////////////////////////////////////////////////////////////////////////
    ///////////////////////// STATIC DISPLAY ELEMENTS //////////////////////
    ////////////////////////////////////////////////////////////////////////
    // Configure text appearance
    setTextSize(1) ;
    setTextColor(WHITE);

    // Draw bottom plot axis (PWM duty cycle: 0.0 to 1.0)
    drawHLine(75, 430, 5, CYAN) ;  // Tick mark at bottom (0.0)
    drawHLine(75, 355, 5, CYAN) ;  // Tick mark at middle (0.5)
    drawHLine(75, 280, 5, CYAN) ;  // Tick mark at top (1.0)
    drawVLine(80, 280, 150, CYAN) ; // Vertical axis line
    
    // Label bottom plot Y-axis values
    sprintf(screentext, "0.5") ;
    setCursor(50, 350) ;
    writeString(screentext) ;
    sprintf(screentext, "1.0") ;
    setCursor(50, 280) ;
    writeString(screentext) ;
    sprintf(screentext, "0.0") ;
    setCursor(50, 425) ;
    writeString(screentext) ;

    // Draw top plot axis (Beam angle: 0 to 180 degrees)
    drawHLine(75, 230, 5, CYAN) ;  // Tick mark at bottom (0°)
    drawHLine(75, 155, 5, CYAN) ;  // Tick mark at middle (90°)
    drawHLine(75, 80, 5, CYAN) ;   // Tick mark at top (180°)
    drawVLine(80, 80, 150, CYAN) ;  // Vertical axis line
    
    // Label top plot Y-axis values
    sprintf(screentext, "90") ;
    setCursor(50, 150) ;
    writeString(screentext) ;
    sprintf(screentext, "180") ;
    setCursor(45, 75) ;
    writeString(screentext) ;
    sprintf(screentext, "0") ;
    setCursor(45, 225) ;
    writeString(screentext) ;

    ////////////////////////////////////////////////////////////////////////
    ///////////////////////// MAIN DISPLAY LOOP ////////////////////////////
    ////////////////////////////////////////////////////////////////////////
    while (true) {
        // Wait for signal from ISR that new data is available
        PT_SEM_WAIT(pt, &vga_semaphore);
        
        // Increment throttle counter
        throttle += 1 ;
        
        // Only update display when throttle reaches threshold
        // This controls the scrolling speed of the plots
        if (throttle >= threshold) { 
            // Reset throttle for next cycle
            throttle = 0 ;

            // Clear the current column (prepare for new data)
            drawVLine(xcoord, 80, 480, BLACK) ;

            // Draw bottom plot: PWM duty cycle (0.0 to 1.0)
            // Yellow pixel shows filtered motor control output
            // Scale: control_filtered (0-5000) → duty cycle (0-500) → pixel position
            drawPixel(xcoord, 430 - (int)(NewRange*((float)(control_filtered*500/5000)/OldRange)), YELLOW) ;

            // Draw top plot: Complementary filter angle estimate (0-180°)
            // Yellow pixel shows current beam angle from sensor fusion
            // Scale: complementary_angle (0-180°) → normalized (0-500) → pixel position
            drawPixel(xcoord, 230 - (int)(NewRange*((float)(fix2float15(complementary_angle)*500.0/180.0)/OldRange)), YELLOW) ;
            
            // Draw target angle reference line in cyan
            // This shows where the controller is trying to position the beam
            drawPixel(xcoord, 230 - (int)(NewRange*((float)(fix2float15(target_angle)*500.0/180.0)/OldRange)), CYAN) ;

            // Advance to next column for scrolling effect
            if (xcoord < 609) {
                xcoord += 1 ;  // Move right
            }
            else {
                xcoord = 81 ;  // Wrap around to left edge when reaching right side
            }
        }

        ////////////////////////////////////////////////////////////////////
        ///////////////////////// DISPLAY LABELS //////////////////////////
        ////////////////////////////////////////////////////////////////////
        // Display plot titles (updated continuously)
        sprintf(screentext, "Beam angle (degrees)") ;
        setCursor(240, 55) ;
        writeString(screentext);
        
        sprintf(screentext, "PWM duty cycle (0.0-1.0)") ;
        setCursor(240, 260) ;
        writeString(screentext);

        // Update parameter display when PID values or target angle changes
        // This avoids flickering by only redrawing when necessary
        if ( parameter_update ) {
            parameter_update = 0 ;  // Clear the update flag
            
            // Erase old parameter text area
            fillRect(0, 0, 180, 70, BLACK) ;
            
            // Display current control parameters
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

////////////////////////////////////////////////////////////////////////
///////////////////////// SERIAL INPUT THREAD //////////////////////////
////////////////////////////////////////////////////////////////////////
/**
 * Serial Input Protothread
 * 
 * Provides a command-line interface via USB serial for real-time tuning.
 * Users can adjust:
 * - Target beam angle (0-180 degrees)
 * - PID controller gains (Kp, Ki, Kd)
 * - Display update speed (throttle threshold)
 */
static PT_THREAD (protothread_serial(struct pt *pt))
{
    PT_BEGIN(pt) ;
    
    // Local variables for parsing user input
    static char classifier ;   // Command character entered by user
    static int test_in ;       // Integer input buffer
    static float float_in ;    // Floating-point input buffer
    
    while(1) {
        // Display menu of available commands
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
        
        // Wait for user to enter a command character
        serial_read;
        sscanf(pt_serial_in_buffer,"%c", &classifier);
        ////////////////////////////////////////////////////////////////////
        ///////////////////////// COMMAND HANDLING ////////////////////////
        ////////////////////////////////////////////////////////////////////
        // Command 'a': Set target angle
        if (classifier=='a') {
            sprintf(pt_serial_out_buffer, "Input target angle (0-180): ");
            serial_write;
            serial_read;
            sscanf(pt_serial_in_buffer,"%d", &test_in);
            
            // Validate input range
            if (test_in > 180) continue;       // Reject if too large
            else if (test_in < 0) continue;    // Reject if negative
            else target_angle = int2fix15(test_in);  // Accept and convert
            parameter_update = 1;  // Trigger display update
        // Command 'p': Set proportional gain
        } else if (classifier=='p') {
            sprintf(pt_serial_out_buffer, "Input Kp (float): ");
            serial_write;
            serial_read;
            sscanf(pt_serial_in_buffer,"%f", &float_in);
            
            // Reject negative gains
            if (float_in < 0) continue;
            else Kp = float2fix15(float_in);
            parameter_update = 1;
        // Command 'i': Set integral gain
        } else if (classifier=='i') {
            sprintf(pt_serial_out_buffer, "Input Ki (float): ");
            serial_write;
            serial_read;
            sscanf(pt_serial_in_buffer,"%f", &float_in);
            
            // Reject negative gains
            if (float_in < 0) continue;
            else Ki = float2fix15(float_in);
            parameter_update = 1;
        // Command 'd': Set derivative gain
        } else if (classifier=='d') {
            sprintf(pt_serial_out_buffer, "Input Kd (float): ");
            serial_write;
            serial_read;
            sscanf(pt_serial_in_buffer,"%f", &float_in);
            
            // Reject negative gains
            if (float_in < 0) continue;
            else Kd = float2fix15(float_in);
            parameter_update = 1;
        // Command 'r': Reset PID parameters to default values
        } else if (classifier=='r') {
            Kp = float2fix15(50.0);  // Default proportional gain
            Ki = float2fix15(50.0);  // Default integral gain
            Kd = float2fix15(15.0);  // Default derivative gain
            parameter_update = 1;
        // Command 'c': Print current PID parameters
        } else if (classifier=='c') {
            sprintf(pt_serial_out_buffer, "Current PID parameters:\r\n");
            serial_write;
            sprintf(pt_serial_out_buffer, "Kp: %.2f\r\n", fix2float15(Kp));
            serial_write;
            sprintf(pt_serial_out_buffer, "Ki: %.2f\r\n", fix2float15(Ki));
            serial_write;
            sprintf(pt_serial_out_buffer, "Kd: %.2f\r\n", fix2float15(Kd));
            serial_write;
        // Command 't': Set display throttle (controls update speed)
        } else if ( classifier=='t') {
            sprintf(pt_serial_out_buffer, "Input threshold (1-100): ");
            serial_write;
            serial_read;
            sscanf(pt_serial_in_buffer,"%d", &test_in);
            
            // Validate range (1-100)
            if (test_in < 1) continue;
            else if (test_in > 100) continue;
            else threshold = test_in ;  // Higher = slower display updates
        // Invalid command
        } else {
            sprintf(pt_serial_out_buffer, "Invalid command\r\n");
            serial_write;
        }

    }
    PT_END(pt) ;
}

////////////////////////////////////////////////////////////////////////
///////////////////////// MULTICORE ENTRY POINT ////////////////////////
////////////////////////////////////////////////////////////////////////
/**
 * Core 1 Entry Point
 * 
 * Core 1 handles VGA display updates. It runs the VGA protothread which
 * waits for signals from the ISR on Core 0, then draws the plots.
 */
void core1_entry() {
    pt_add_thread(protothread_vga) ;  // Register VGA thread
    pt_schedule_start ;                // Start protothread scheduler (never returns)
}

////////////////////////////////////////////////////////////////////////
///////////////////////// MAIN FUNCTION ////////////////////////////////
////////////////////////////////////////////////////////////////////////
/**
 * Main Function (runs on Core 0)
 * 
 * Initializes all hardware peripherals and starts the control system:
 * 1. Overclocks system to 150MHz for performance
 * 2. Configures button input with interrupts
 * 3. Initializes I2C and MPU6050 IMU sensor
 * 4. Configures PWM for motor control with 1kHz interrupt
 * 5. Launches VGA thread on Core 1
 * 6. Runs serial input thread on Core 0
 */
int main() {

    // Overclock system clock to 150MHz for better performance
    set_sys_clock_khz(150000, true) ;

    // Initialize USB serial communication
    stdio_init_all();

    // Initialize VGA display hardware and graphics library
    initVGA() ;

    ////////////////////////////////////////////////////////////////////////
    ///////////////////////// BUTTON CONFIGURATION /////////////////////////
    ////////////////////////////////////////////////////////////////////////
    // Configure GPIO pin for button input
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);  // Set as input
    gpio_pull_up(BUTTON_PIN);           // Enable internal pull-up resistor

    // Enable interrupts on both edges to detect press and release events
    // Falling edge = button pressed, Rising edge = button released
    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &button_irq_handler);

    ////////////////////////////////////////////////////////////////////////
    ///////////////////////// I2C CONFIGURATION ////////////////////////////
    ////////////////////////////////////////////////////////////////////////
    // Initialize I2C bus for MPU6050 communication
    i2c_init(I2C_CHAN, I2C_BAUD_RATE) ;
    
    // Configure GPIO pins for I2C function
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C) ;  // Data line
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C) ;  // Clock line

    // Note: MPU6050 breakout board has external pull-up resistors
    // Internal pull-ups not needed:
    // gpio_pull_up(SDA_PIN) ;
    // gpio_pull_up(SCL_PIN) ;

    // Initialize MPU6050 IMU sensor
    mpu6050_reset();                          // Reset sensor to known state
    mpu6050_read_raw(acceleration, gyro);    // Prime the data arrays

    ////////////////////////////////////////////////////////////////////////
    ///////////////////////// PWM CONFIGURATION ////////////////////////////
    ////////////////////////////////////////////////////////////////////////
    // Configure GPIO pins for PWM output to motor driver
    gpio_set_function(5, GPIO_FUNC_PWM);  // PWM output pin 1
    gpio_set_function(4, GPIO_FUNC_PWM);  // PWM output pin 2

    // Determine which PWM slice controls these pins
    // GPIOs 4 and 5 are both on slice 2
    slice_num = pwm_gpio_to_slice_num(5);

    // Configure PWM interrupt to trigger at 1kHz for control loop
    pwm_clear_irq(slice_num);                               // Clear any pending interrupts
    pwm_set_irq_enabled(slice_num, true);                   // Enable interrupts for this slice
    irq_set_exclusive_handler(PWM_IRQ_WRAP, on_pwm_wrap);   // Register ISR
    irq_set_enabled(PWM_IRQ_WRAP, true);                    // Enable interrupt in NVIC

    // Configure PWM period
    // Period = (WRAPVAL * CLKDIV) / system_clock
    // With 150MHz clock: (5000 * 25) / 150MHz = 1kHz
    pwm_set_wrap(slice_num, WRAPVAL) ;    // Counter wraps at 5000
    pwm_set_clkdiv(slice_num, CLKDIV) ;   // Divide clock by 25.0

    // Initialize duty cycle to 0% (motor off)
    pwm_set_chan_level(slice_num, PWM_CHAN_B, 0);
    pwm_set_chan_level(slice_num, PWM_CHAN_A, 0);

    // Enable the PWM slice to start generating signals
    pwm_set_mask_enabled((1u << slice_num));


    ////////////////////////////////////////////////////////////////////////
    ///////////////////////////// START SYSTEM /////////////////////////////
    ////////////////////////////////////////////////////////////////////////
    // Launch Core 1 to handle VGA display
    multicore_reset_core1();                  // Reset Core 1 to clean state
    multicore_launch_core1(core1_entry);      // Start VGA thread on Core 1

    // Start serial input thread on Core 0
    pt_add_thread(protothread_serial) ;       // Register serial thread
    pt_schedule_start ;                       // Start scheduler (never returns)
    // Control system is now running:
    // - Core 0: PWM ISR runs control loop at 1kHz + serial input thread
    // - Core 1: VGA display updates synchronized with ISR
}
