/**
 * DIGITAL GALTON BOARD SIMULATION
 * Hunter Adams (vha3@cornell.edu)
 * 
 * This program simulates a Galton Board (also known as a bean machine or quincunx),
 * which demonstrates the Central Limit Theorem and the emergence of a Gaussian
 * distribution from repeated Bernoulli trials.
 *
 * PHYSICS MODEL:
 * - Balls drop from the top of the screen and bounce off pegs arranged in rows
 * - Each collision follows elastic collision physics with coefficient of restitution
 * - Gravity accelerates balls downward
 * - Balls respawn at the top after falling through the bottom
 * - A histogram at the bottom tracks the distribution of ball landing positions
 *
 * OPTIMIZATION TECHNIQUES:
 * - Fixed-point arithmetic (15-bit fractional) for fast integer-based calculations
 * - Alpha max plus beta min algorithm for fast distance approximation
 * - Dual-core parallelism (RP2040 cores 0 and 1 each handle half the balls)
 * - DMA-based audio synthesis for collision sound effects (no CPU overhead)
 * - Overclocked to 250 MHz for maximum performance
 *
 * USER INTERFACE:
 * - Button cycles through control modes: None -> Ball Count -> Bounciness -> Gravity
 * - Potentiometer adjusts the selected parameter in real-time
 * - VGA display shows current parameters, ball count, and elapsed time
 * - LED indicator lights when frame rate deadline (30 fps) is missed
 *
 * HARDWARE CONNECTIONS:
 *  VGA Output:
 *   - GPIO 16 ---> Pin 21 ---> VGA Hsync
 *   - GPIO 17 ---> Pin 22 ---> VGA Vsync
 *   - GPIO 18 ---> Pin 24 ---> VGA Green lo-bit --> 470 ohm resistor --> VGA_Green
 *   - GPIO 19 ---> Pin 25 ---> VGA Green hi_bit --> 330 ohm resistor --> VGA_Green
 *   - GPIO 20 ---> Pin 26 ---> 330 ohm resistor ---> VGA-Blue
 *   - GPIO 21 ---> Pin 27 ---> 330 ohm resistor ---> VGA-Red
 *   - RP2040 GND ---> Pin 23 ---> VGA-GND
 *  Audio DAC (SPI):
 *   - GPIO 4 ---> MISO
 *   - GPIO 5 ---> CS (Chip Select)
 *   - GPIO 6 ---> SCK (Clock)
 *   - GPIO 7 ---> MOSI (Data)
 *  User Input:
 *   - GPIO 2 ---> Button (with pull-up)
 *   - GPIO 26 ---> Potentiometer (ADC input)
 *  Status:
 *   - GPIO 25 ---> LED (frame rate indicator)
 *
 * RESOURCES USED:
 *  - PIO state machines 0, 1, and 2 on PIO instance 0 (VGA signal generation)
 *  - DMA channels 2 (data and control, for audio synthesis)
 *  - 153.6 kBytes of RAM (for pixel color data)
 *  - Both CPU cores (multicore parallelism)
 *  - ADC channel 0 (potentiometer input)
 *  - SPI channel 0 (DAC output for audio)
 *
 */

// ===========================
// ===== LIBRARY INCLUDES ====
// ===========================

// VGA graphics library for 16-color display output
#include "vga16_graphics_v2.h"

// Standard C libraries
#include <stdio.h>   // For sprintf and printf functions
#include <stdlib.h>  // For rand() and general utilities
#include <math.h>    // For sin() function (used in sine table generation)
#include <string.h>  // For string manipulation

// Pico SDK core libraries
#include "pico/stdlib.h"     // Core Pico functionality
#include "pico/divider.h"    // Hardware divider for fixed-point division
#include "pico/multicore.h"  // Dual-core support

// Hardware peripheral libraries
#include "hardware/pio.h"     // Programmable I/O (for VGA timing)
#include "hardware/dma.h"     // Direct Memory Access (for audio)
#include "hardware/clocks.h"  // System clock configuration
#include "hardware/pll.h"     // Phase-Locked Loop (for overclocking)
#include "hardware/spi.h"     // SPI interface (for DAC)
#include "hardware/adc.h"     // Analog-to-Digital Converter (for potentiometer)
#include "hardware/gpio.h"    // General Purpose I/O

// Protothreads library for cooperative multitasking
#include "pt_cornell_rp2040_v1_4.h"

// ===================================
// ===== PHYSICS PARAMETER DEFAULTS ==
// ===================================
// These values can be adjusted via potentiometer during runtime

int ball_num_total = 1400;       // Total number of balls to animate (split across both cores)
float bounciness_float = 0.38;   // Coefficient of restitution (0-1): energy retained after collision
float g_float = 0.75;            // Gravity acceleration (pixels per frame squared)

// ===============================
// ===== ADC (POTENTIOMETER) =====
// ===============================
// The potentiometer provides analog input for adjusting parameters

#define ADC_PIN 26  // GPIO 26 is ADC channel 0

int adc_value_raw;                      // Raw 12-bit ADC reading (0-4095)
int adc_value_32;                       // Scaled ADC value (1-32) for parameter adjustment
int adc_value_raw_history[10] = {0};    // Circular buffer for ADC readings (for change detection)
int adc_history_index = 0;              // Current index in circular buffer

bool reset = false;  // Flag to reset histogram when parameter changes significantly

// ============================
// ===== CONTROL STATE FSM ====
// ============================
// Finite state machine for user interface control modes
// Button press cycles through these states

#define CTRL_NONE        0  // No parameter control (potentiometer inactive)
#define CTRL_BALL_NUM    1  // Potentiometer adjusts number of balls
#define CTRL_BOUNCINESS  2  // Potentiometer adjusts coefficient of restitution
#define CTRL_GRAVITY     3  // Potentiometer adjusts gravity strength

volatile int ctrl_state = CTRL_NONE;  // Current control state (volatile for ISR safety)
int ctrl_state_num = 4;               // Total number of control states

// ========================
// ===== GPIO BUTTON ======
// ========================
// Button for cycling through control modes

#define BUTTON_PIN  2  // GPIO 2 with internal pull-up resistor

volatile bool button_not_pushed = true;       // Current button state (true = not pressed)
volatile bool button_not_pushed_prev = true;  // Previous button state (for edge detection)

// ====================
// ===== LED PIN ======
// ====================
// LED indicates when 30fps frame rate deadline is missed

#define LED_PIN 25  // Built-in LED on Pico board

// ============================================
// ====== DMA-BASED AUDIO SYNTHESIS ===========
// ============================================
// This section implements collision sound effects using DMA to drive an SPI DAC.
// The DMA approach eliminates CPU overhead - sound plays automatically without
// interrupt handlers or polling.
//
// AUDIO ARCHITECTURE:
// 1. A sine wave table is pre-computed and stored in DAC_data[]
// 2. Control DMA channel holds the address of the data table
// 3. Data DMA channel streams the sine table to the SPI DAC
// 4. DMA is paced by a hardware timer (~44 kHz sample rate)
// 5. When a ball hits a peg, we trigger the control channel to restart playback
//
// This creates a "thunk" sound effect for each collision.

#define sine_table_size 256  // Number of samples per period in sine wave

int raw_sin[sine_table_size];           // Raw sine values (0-4095 for 12-bit DAC)
unsigned short DAC_data[sine_table_size];  // Formatted data for MCP4822 DAC

// Pointer to the address of the DAC data table (used by control DMA channel)
unsigned short * address_pointer_dma = &DAC_data[0];

// DMA channel handles
int data_chan;  // Data channel: streams sine table to SPI
int ctrl_chan;  // Control channel: reloads data channel address

// MCP4822 DAC configuration word: Channel A, 1x gain, active
#define DAC_config_chan_A 0b0011000000000000

// SPI pin assignments for DAC communication
#define PIN_MISO 4       // Master In Slave Out (unused for DAC)
#define PIN_CS   5       // Chip Select
#define PIN_SCK  6       // Serial Clock
#define PIN_MOSI 7       // Master Out Slave In (data to DAC)
#define SPI_PORT spi0    // Use SPI channel 0

// Number of DMA transfers per sound effect playback
const uint32_t transfer_count = sine_table_size;

// ============================================
// ====== END DMA AUDIO SECTION ===============
// ============================================

// =======================================
// ===== FIXED-POINT ARITHMETIC ==========
// =======================================
// Fixed-point arithmetic is MUCH faster than floating-point on RP2040.
// We use 15-bit fractional precision (Q16.15 format):
//   - Upper 16 bits: integer part
//   - Lower 15 bits: fractional part
//   - 1 sign bit
// This gives us range of approximately -65536 to +65536 with resolution of 1/32768.
//
// WHY FIXED-POINT?
// - Floating-point operations are slow on Cortex-M0+ (no FPU)
// - Integer operations are fast
// - We need fractional precision for smooth animation
// - Fixed-point gives us both speed and precision

typedef signed int fix15;  // 32-bit signed integer used as fixed-point

// Multiply two fix15 numbers (result is also fix15)
#define multfix15(a,b) ((fix15)((((signed long long)(a))*((signed long long)(b)))>>15))

// Convert float to fix15
#define float2fix15(a) ((fix15)((a)*32768.0))  // 2^15 = 32768

// Convert fix15 to float
#define fix2float15(a) ((float)(a)/32768.0)

// Absolute value of fix15
#define absfix15(a) abs(a)

// Convert integer to fix15 (shift left by 15 bits)
#define int2fix15(a) ((fix15)(a << 15))

// Convert fix15 to integer (shift right by 15 bits, truncates fractional part)
#define fix2int15(a) ((int)(a >> 15))

// Convert char to fix15
#define char2fix15(a) (fix15)(((fix15)(a)) << 15)

// Divide two fix15 numbers using hardware divider (fast!)
#define divfix(a,b) (fix15)(div_s64s64( (((signed long long)(a)) << 15), ((signed long long)(b))))

// Square root of fix15 (uses floating-point, slower but needed for distance calculation)
#define sqrtfix(a) (float2fix15(sqrt(fix2float15(a))))

// ===============================
// ===== TIMING AND DISPLAY ======
// ===============================

// Target frame rate: 33000 microseconds = ~30.3 fps
// This gives us 33ms to compute and render each frame
#define FRAME_RATE 33000

// VGA screen resolution (standard 640x480)
#define screen_width   640
#define screen_height  480

// ===============================
// ===== COLOR DEFINITIONS =======
// ===============================

char ball_color_0 =    YELLOW;  // Color for balls on core 0
char ball_color_1 =    BLUE;    // Color for balls on core 1
char peg_color =       GREEN;   // Color for pegs
char histogram_color = WHITE;   // Color for histogram bars

// ===============================
// ===== PHYSICS PARAMETERS ======
// ===============================
// These are the fixed-point versions of the float parameters above

fix15 g;           // Gravity acceleration (fixed-point)
fix15 bounciness;  // Coefficient of restitution (fixed-point)

// ===============================
// ===== BALL PARAMETERS =========
// ===============================
// Balls are split evenly across both CPU cores for parallel processing

#define ball_num_max 1600                   // Maximum number of balls system can handle
#define ball_num_max0 (ball_num_max/2)      // Maximum number of balls on core 0
#define ball_num_max1 ((ball_num_max+1)/2)  // Maximum number of balls on core 1 (+1 for odd total)

int ball_r_int = 4;   // Ball radius in pixels (integer)
fix15 ball_r;         // Ball radius in fixed-point (for collision detection)

// Active ball counts (can be adjusted at runtime)
int ball_num0;       // Current number of balls on core 0 = ball_num_total / 2
int ball_num0_prev;  // Previous frame's ball count (for erasing old positions)
int ball_num1;       // Current number of balls on core 1 = (ball_num_total + 1) / 2
int ball_num1_prev;  // Previous frame's ball count on core 1

// ===============================
// ===== BALL STATE ARRAYS =======
// ===============================
// Each ball has position (x,y), velocity (vx,vy), and collision history

// Core 0 ball arrays
fix15 ball0_x[ball_num_max0];    // X positions (fixed-point)
fix15 ball0_y[ball_num_max0];    // Y positions (fixed-point)
fix15 ball0_vx[ball_num_max0];   // X velocities (fixed-point)
fix15 ball0_vy[ball_num_max0];   // Y velocities (fixed-point)
int ball0_peg_index_prev[ball_num_max0];  // Last peg hit (prevents double-counting collisions)

// Core 1 ball arrays
fix15 ball1_x[ball_num_max1];    // X positions (fixed-point)
fix15 ball1_y[ball_num_max1];    // Y positions (fixed-point)
fix15 ball1_vx[ball_num_max1];   // X velocities (fixed-point)
fix15 ball1_vy[ball_num_max1];   // Y velocities (fixed-point)
int ball1_peg_index_prev[ball_num_max1];  // Last peg hit (prevents double-counting collisions)

// ===============================
// ===== PEG PARAMETERS ==========
// ===============================
// Pegs are arranged in a triangular pattern (Pascal's triangle layout)
// Row 1: 1 peg, Row 2: 2 pegs, Row 3: 3 pegs, etc.
// Total pegs = 1+2+3+...+N = N*(N+1)/2

#define peg_row        16   // Number of rows in the Galton board
#define peg_num        ((peg_row*(peg_row+1))/2)  // Total pegs = 16*17/2 = 136 pegs
#define peg_start_x    screen_width/2   // Top peg X position (centered horizontally)
#define peg_start_y    40               // Top peg Y position (pixels from top)
#define peg_space_x    38               // Horizontal spacing between adjacent pegs
#define peg_space_y    19               // Vertical spacing between rows

int peg_r_int = 6;   // Peg radius in pixels (integer)
fix15 peg_r;         // Peg radius in fixed-point (for collision detection)

// Peg position arrays (computed once at startup)
int peg_x_int[peg_num];    // Integer X positions (for drawing)
int peg_y_int[peg_num];    // Integer Y positions (for drawing)
fix15 peg_x[peg_num];      // Fixed-point X positions (for collision physics)
fix15 peg_y[peg_num];      // Fixed-point Y positions (for collision physics)

// ===============================
// ===== TEXT DISPLAY BUFFERS ====
// ===============================
// String buffers for VGA text output

char text_line1[32];  // Time since boot
char text_line2[32];  // Total balls that have fallen through
char text_line3[32];  // Current number of active balls
char text_line4[32];  // Bounciness parameter value
char text_line5[32];  // Gravity parameter value
char text_line6[32];  // Current control mode

// ===============================
// ===== HISTOGRAM TRACKING ======
// ===============================
// The histogram shows the distribution of ball landing positions,
// demonstrating the Central Limit Theorem in action.
// There are (peg_row - 1) = 15 bins at the bottom of the board.

int fall_count_total = 0;              // Total number of balls that have fallen through
int fall_count[peg_row - 1] = {0};     // Count of balls in each bin (the raw histogram data)
int fall_count_max = 0;                // Maximum count in any bin (for normalization)

// Histogram display parameters
#define histogram_height_max   100     // Maximum height of histogram bars (pixels)
#define histogram_width        peg_space_x  // Width of each histogram bar (matches peg spacing)
#define histogram_start_x      (screen_width/2 - ((peg_row - 1)*histogram_width/2))  // Left edge of histogram

// Normalized histogram heights (scaled to fit in histogram_height_max pixels)
int histogram_height[peg_row - 1] = {0};       // Current normalized heights
int histogram_height_prev0[peg_row - 1] = {0}; // Previous heights (core 0, for erasing)
int histogram_height_prev1[peg_row - 1] = {0}; // Previous heights (core 1, for erasing)

// ===============================================
// ===== INITIALIZATION FUNCTIONS ================
// ===============================================

/**
 * createPeg()
 * 
 * Initializes the peg positions for the Galton board.
 * Pegs are arranged in a triangular pattern:
 *   Row 1: 1 peg (centered)
 *   Row 2: 2 pegs
 *   Row 3: 3 pegs
 *   ...
 *   Row N: N pegs
 * 
 * Each row is centered horizontally and spaced vertically.
 * This creates the classic Galton board geometry where balls
 * bounce left or right at each peg encounter.
 */
void createPeg()
{
  int peg_index = 0;
  
  // Iterate through each row of pegs
  for (int r = 1; r <= peg_row; r++)
  {
    // Each row r has r pegs
    for (int i = 1; i <= r; i++)
    {
      // Calculate X position: center the row, then space pegs evenly
      peg_x_int[peg_index] = peg_start_x - peg_space_x*(r-1)/2 + peg_space_x*(i-1);
      
      // Calculate Y position: each row is spaced vertically
      peg_y_int[peg_index] = peg_start_y + peg_space_y*(r-1);
      
      peg_index += 1;
    }
  }

  // Convert integer positions to fixed-point for physics calculations
  for (int i = 0; i < peg_num; i++)
  {
    peg_x[i] = int2fix15(peg_x_int[i]);
    peg_y[i] = int2fix15(peg_y_int[i]);
  }
}

/**
 * createBall0()
 * 
 * Initializes all balls for core 0.
 * Each ball starts at the top center of the screen with:
 *   - Zero vertical velocity (dropped, not thrown)
 *   - Small random horizontal velocity (to break symmetry)
 * 
 * The random horizontal velocity ensures balls don't all follow
 * identical paths, which would defeat the statistical demonstration.
 */
void createBall0()
{
  for (int i = 0; i < ball_num_max0; i++)
  {
    // Start at horizontal center, just below top edge
    ball0_x[i] = int2fix15(screen_width/2);
    ball0_y[i] = int2fix15(ball_r_int);
    
    // Small random horizontal velocity (breaks left-right symmetry)
    ball0_vx[i] = (fix15)((rand() & 0xffff) - int2fix15(1));
    
    // Zero initial vertical velocity (balls are dropped, not thrown)
    ball0_vy[i] = int2fix15(0);
  }
}

/**
 * createBall1()
 * 
 * Initializes all balls for core 1.
 * Identical to createBall0() but operates on core 1's ball arrays.
 * See createBall0() for detailed comments.
 */
// Create balls on core 1
void createBall1()
{
  // Start in center top
  for (int i = 0; i < ball_num_max1; i++)
  {
    ball1_x[i] = int2fix15(screen_width/2);
    ball1_y[i] = int2fix15(ball_r_int);
    ball1_vx[i] = (fix15)((rand() & 0xffff) - int2fix15(1));
    ball1_vy[i] = int2fix15(0);
  }
}

// ===============================================
// ===== PHYSICS AND ANIMATION FUNCTIONS =========
// ===============================================

/**
 * moveBall0()
 * 
 * Updates physics for all balls on core 0. This is the heart of the simulation.
 * For each ball, we:
 *   1. Check collisions with all pegs
 *   2. Apply elastic collision physics when ball hits a peg
 *   3. Apply coefficient of restitution (energy loss)
 *   4. Trigger sound effect on new peg collisions
 *   5. Handle wall bounces
 *   6. Respawn balls that fall through the bottom
 *   7. Update histogram when balls land
 *   8. Apply gravity
 *   9. Update position based on velocity
 * 
 * OPTIMIZATION NOTES:
 * - Uses fixed-point arithmetic throughout for speed
 * - Alpha max plus beta min approximation for distance (avoids sqrt)
 * - Early exit from peg loop after first collision per frame
 * - Inline function to eliminate call overhead
 */

// Update ball position and velocity on core 0
static inline void moveBall0()
{
  // Iterate through each ball assigned to this core
  for (int i = 0; i < ball_num0; i++)
  {
    // Iterate through each peg to check for collisions
    for (int j = 0; j < peg_num; j++)
    {
      // --- PEG COLLISION DETECTION ---
      // Calculate the vector from the peg center to the ball center
      fix15 dx = ball0_x[i] - peg_x[j];
      fix15 dy = ball0_y[i] - peg_y[j];

      // AABB (Axis-Aligned Bounding Box) check for coarse collision detection.
      // This is a fast way to rule out collisions with distant pegs.
      if ( (abs(dx) < ball_r + peg_r) && (abs(dy) < ball_r + peg_r) )
      {
        // --- DISTANCE APPROXIMATION ---
        // If the AABB check passes, do a more precise distance check.
        // The commented-out line is the true distance calculation using sqrt, which is slow.
        // fix15 distance = sqrtfix(multfix15(dx, dx) + multfix15(dy, dy));

        // Alpha max plus beta min algorithm: a fast approximation for sqrt(a^2 + b^2).
        // It's much faster than a true sqrt on this hardware.
        fix15 max, min;
        if (absfix15(dx) > absfix15(dy)) {
          max = absfix15(dx);
          min = absfix15(dy);
        } else {
          max = absfix15(dy);
          min = absfix15(dx);
        }
        fix15 distance = max + (min >> 2); // distance ≈ max + 0.25*min

        // --- COLLISION RESPONSE ---
        // Normalize the collision vector (dx, dy) to get the normal vector
        fix15 normal_x = divfix(dx, distance);
        fix15 normal_y = divfix(dy, distance);

        // Elastic collision formula: v_new = v_old - 2 * dot(v_old, n) * n
        // First, calculate the projection of velocity onto the normal: dot(v, n)
        // The intermediate term is -2 * dot(v, n)
        fix15 intermediate_term = multfix15(int2fix15(-2), 
          (multfix15(normal_x,  ball0_vx[i]) + multfix15(normal_y, ball0_vy[i])));

        // Check if the ball is moving towards the peg (if not, it's moving away, no collision)
        if (intermediate_term > int2fix15(0))
        {
          // --- RESOLVE OVERLAP ---
          // Move the ball so it's just touching the peg, to prevent sinking.
          ball0_x[i] = peg_x[j] + multfix15(normal_x, (ball_r + peg_r + int2fix15(1)));
          ball0_y[i] = peg_y[j] + multfix15(normal_y, (ball_r + peg_r + int2fix15(1)));

          // --- UPDATE VELOCITY ---
          // Apply the elastic collision formula to reflect the velocity
          ball0_vx[i] = ball0_vx[i] + multfix15(normal_x, intermediate_term);
          ball0_vy[i] = ball0_vy[i] + multfix15(normal_y, intermediate_term);

          // Check if this is a new collision with a different peg
          if ( j != ball0_peg_index_prev[i] )
          {
            // Record the new peg index to prevent multiple bounces on the same peg
            ball0_peg_index_prev[i] = j;

            // --- APPLY BOUNCINESS ---
            // Reduce velocity by the bounciness factor (coefficient of restitution)
            ball0_vx[i] = multfix15(ball0_vx[i], bounciness);
            ball0_vy[i] = multfix15(ball0_vy[i], bounciness);

            // --- TRIGGER SOUND ---
            // Trigger the DMA audio channel to play a sound, if it's not already playing.
            if (!dma_channel_is_busy(data_chan)) {
              dma_start_channel_mask(1u << ctrl_chan);
            }

            // Optimization: Assume only one collision per frame. Exit the peg loop.
            break;
          }
        }
      }
    }

    // --- WALL COLLISION ---
    // Check for collisions with the left and right screen edges
    if ( (fix2int15(ball0_x[i]) < ball_r_int) || (fix2int15(ball0_x[i]) > (screen_width - ball_r_int)) )
    {
      // Reverse horizontal velocity
      ball0_vx[i] = -ball0_vx[i];
    }
    // Check for collision with the top of the screen
    if ( fix2int15(ball0_y[i]) < ball_r_int )
    {
      // Reverse vertical velocity
      ball0_vy[i] = -ball0_vy[i];
    }

    // --- BALL RESPAWN ---
    // Check if the ball has fallen past the pegs and into the histogram area
    if ( fix2int15(ball0_y[i]) > (screen_height - histogram_height_max - 40) )
    {
      // Increment total number of fallen balls
      fall_count_total += 1;

      // --- UPDATE HISTOGRAM ---
      // Find which histogram bin the ball landed in
      for (int p = 0; p < peg_row - 1; p++)
      {
        if ( (fix2int15(ball0_x[i]) >= (histogram_start_x + p*histogram_width)) && 
             (fix2int15(ball0_x[i]) < (histogram_start_x + (p+1)*histogram_width)) )
        {
          // Increment the count for that bin
          fall_count[p] += 1;
          // Update the maximum count if this bin is now the highest
          if ( fall_count[p] > fall_count_max )
          {
            fall_count_max = fall_count[p];
          }
          break;
        }
      }

      // Normalize all histogram bars to the maximum height
      for (int p = 0; p < peg_row - 1; p++)
      {
        histogram_height[p] = (fall_count[p] * histogram_height_max) / fall_count_max;
      }

      // --- RESET BALL STATE ---
      // Respawn the ball at the top center
      ball0_x[i] = int2fix15(screen_width/2);
      ball0_y[i] = int2fix15(ball_r_int);
      // Give it a small random horizontal velocity to vary its path
      ball0_vx[i] = (fix15)((rand() & 0xffff) - int2fix15(1));
      // Reset vertical velocity to zero
      ball0_vy[i] = int2fix15(0);
    }

    // --- APPLY GRAVITY ---
    // Accelerate the ball downwards
    ball0_vy[i] = ball0_vy[i] + g;

    // --- UPDATE POSITION ---
    // Update position based on the new velocity
    ball0_x[i] = ball0_x[i] + ball0_vx[i];
    ball0_y[i] = ball0_y[i] + ball0_vy[i];
  }
}

// Update ball position and velocity on core 1
static inline void moveBall1()
{
  // Iterate through each ball assigned to this core
  for (int i = 0; i < ball_num1; i++)
  {
    // Iterate through each peg to check for collisions
    for (int j = 0; j < peg_num; j++)
    {
      // --- PEG COLLISION DETECTION ---
      // Calculate the vector from the peg center to the ball center
      fix15 dx = ball1_x[i] - peg_x[j];
      fix15 dy = ball1_y[i] - peg_y[j];

      // AABB (Axis-Aligned Bounding Box) check for coarse collision detection.
      // This is a fast way to rule out collisions with distant pegs.
      if ( (abs(dx) < ball_r + peg_r) && (abs(dy) < ball_r + peg_r) )
      {
        // --- DISTANCE APPROXIMATION ---
        // If the AABB check passes, do a more precise distance check.
        // The commented-out line is the true distance calculation using sqrt, which is slow.
        // fix15 distance = sqrtfix(multfix15(dx, dx) + multfix15(dy, dy));

        // Alpha max plus beta min algorithm: a fast approximation for sqrt(a^2 + b^2).
        // It's much faster than a true sqrt on this hardware.
        fix15 max, min;
        if (absfix15(dx) > absfix15(dy)) {
          max = absfix15(dx);
          min = absfix15(dy);
        } else {
          max = absfix15(dy);
          min = absfix15(dx);
        }
        fix15 distance = max + (min >> 2); // distance ≈ max + 0.25*min

        // --- COLLISION RESPONSE ---
        // Normalize the collision vector (dx, dy) to get the normal vector
        fix15 normal_x = divfix(dx, distance);
        fix15 normal_y = divfix(dy, distance);

        // Elastic collision formula: v_new = v_old - 2 * dot(v_old, n) * n
        // First, calculate the projection of velocity onto the normal: dot(v, n)
        // The intermediate term is -2 * dot(v, n)
        fix15 intermediate_term = multfix15(int2fix15(-2), 
          (multfix15(normal_x,  ball1_vx[i]) + multfix15(normal_y, ball1_vy[i])));

        // Check if the ball is moving towards the peg (if not, it's moving away, no collision)
        if (intermediate_term > int2fix15(0))
        {
          // --- RESOLVE OVERLAP ---
          // Move the ball so it's just touching the peg, to prevent sinking.
          ball1_x[i] = peg_x[j] + multfix15(normal_x, (ball_r + peg_r + int2fix15(1)));
          ball1_y[i] = peg_y[j] + multfix15(normal_y, (ball_r + peg_r + int2fix15(1)));

          // --- UPDATE VELOCITY ---
          // Apply the elastic collision formula to reflect the velocity
          ball1_vx[i] = ball1_vx[i] + multfix15(normal_x, intermediate_term);
          ball1_vy[i] = ball1_vy[i] + multfix15(normal_y, intermediate_term);

          // Check if this is a new collision with a different peg
          if ( j != ball1_peg_index_prev[i] )
          {
            // Record the new peg index to prevent multiple bounces on the same peg
            ball1_peg_index_prev[i] = j;

            // --- APPLY BOUNCINESS ---
            // Reduce velocity by the bounciness factor (coefficient of restitution)
            ball1_vx[i] = multfix15(ball1_vx[i], bounciness);
            ball1_vy[i] = multfix15(ball1_vy[i], bounciness);

            // --- TRIGGER SOUND ---
            // Trigger the DMA audio channel to play a sound, if it's not already playing.
            if (!dma_channel_is_busy(data_chan)) {
              dma_start_channel_mask(1u << ctrl_chan);
            }

            // Optimization: Assume only one collision per frame. Exit the peg loop.
            break;
          }
        }
      }
    }

    // --- WALL COLLISION ---
    // Check for collisions with the left and right screen edges
    if ( (fix2int15(ball1_x[i]) < ball_r_int) || (fix2int15(ball1_x[i]) > (screen_width - ball_r_int)) )
    {
      // Reverse horizontal velocity
      ball1_vx[i] = -ball1_vx[i];
    }
    // Check for collision with the top of the screen
    if ( fix2int15(ball1_y[i]) < ball_r_int )
    {
      // Reverse vertical velocity
      ball1_vy[i] = -ball1_vy[i];
    }

    // --- BALL RESPAWN ---
    // Check if the ball has fallen past the pegs and into the histogram area
    if ( fix2int15(ball1_y[i]) > (screen_height - histogram_height_max - 40) )
    {
      // Increment total number of fallen balls
      fall_count_total += 1;

      // --- UPDATE HISTOGRAM ---
      // Find which histogram bin the ball landed in
      for (int p = 0; p < peg_row - 1; p++)
      {
        if ( (fix2int15(ball1_x[i]) >= (histogram_start_x + p*histogram_width)) && 
             (fix2int15(ball1_x[i]) < (histogram_start_x + (p+1)*histogram_width)) )
        {
          // Increment the count for that bin
          fall_count[p] += 1;
          // Update the maximum count if this bin is now the highest
          if ( fall_count[p] > fall_count_max )
          {
            fall_count_max = fall_count[p];
          }
          break;
        }
      }

      // Normalize all histogram bars to the maximum height
      for (int p = 0; p < peg_row - 1; p++)
      {
        histogram_height[p] = (fall_count[p] * histogram_height_max) / fall_count_max;
      }

      // --- RESET BALL STATE ---
      // Respawn the ball at the top center
      ball1_x[i] = int2fix15(screen_width/2);
      ball1_y[i] = int2fix15(ball_r_int);
      // Give it a small random horizontal velocity to vary its path
      ball1_vx[i] = (fix15)((rand() & 0xffff) - int2fix15(1));
      // Reset vertical velocity to zero
      ball1_vy[i] = int2fix15(0);
    }

    // --- APPLY GRAVITY ---
    // Accelerate the ball downwards
    ball1_vy[i] = ball1_vy[i] + g;

    // --- UPDATE POSITION ---
    // Update position based on the new velocity
    ball1_x[i] = ball1_x[i] + ball1_vx[i];
    ball1_y[i] = ball1_y[i] + ball1_vy[i];
  }
}
// ==================================================
// === users serial input thread
// ==================================================
// This thread handles serial input from the user, allowing them to change
// the color of the balls on core 0 in real-time.
static PT_THREAD (protothread_serial(struct pt *pt))
{
    PT_BEGIN(pt);

    // Announce the protothreads version to the serial console
    sprintf(pt_serial_out_buffer, "Protothreads RP2040 v1.0\n\r");
    serial_write; // Non-blocking write

    // Main loop for continuously reading serial input
    while(1) {
      // Prompt the user for input
      sprintf(pt_serial_out_buffer, "input a number in the range 1-15: ");
      serial_write; // Non-blocking write

      // Wait for user to enter a line of text
      serial_read;

      // --- PARSE USER INPUT ---
      // Static variable to hold the parsed integer from the user
      static int user_input;
      // sscanf parses the input buffer and converts it to an integer
      sscanf(pt_serial_in_buffer, "%d", &user_input);

      // --- UPDATE BALL COLOR ---
      // Check if the input is a valid color (1-15)
      if ((user_input > 0) && (user_input < 16)) {
        // If valid, cast the integer to a char and update the ball color
        ball_color_0 = (char)user_input;
      }

      // Yield for a short time to prevent this thread from hogging the CPU
      PT_YIELD_usec(100000);
    } // END WHILE(1)
  PT_END(pt);
} // timer thread

// Animation on core 0
// This is the main animation thread for core 0. It runs in a tight loop
// to handle user input, update game state, and render the scene.
static PT_THREAD (protothread_anim0(struct pt *pt))
{
    // Mark beginning of thread
    PT_BEGIN(pt);

    // Variables for maintaining a consistent frame rate
    static int begin_time; // Timestamp at the start of a frame
    static int spare_time; // Time left over at the end of a frame

    // Main animation loop
    while(1) {
      // --- FRAME TIMING ---
      // Record the start time of the frame
      begin_time = time_us_32();

      // --- USER INPUT: POTENTIOMETER (ADC) ---
      // Read the raw 12-bit value from the ADC connected to the potentiometer
      adc_value_raw = adc_read();
      // Store the reading in a circular history buffer. This is used to detect
      // if the potentiometer value has changed significantly.
      adc_value_raw_history[adc_history_index] = adc_value_raw;
      int oldest_index = (adc_history_index + 1) % 10;

      // Check for a large change in ADC value to reset the histogram
      if ( abs(adc_value_raw - adc_value_raw_history[oldest_index]) > 200 )
      {
        // Only reset if a parameter is actively being controlled
        if ( ctrl_state != CTRL_NONE )
        {
          reset = true;
        }
      } else 
      {
        reset = false;
      }
      // Advance the history buffer index
      adc_history_index = (adc_history_index + 1) % 10;

      // If a reset is triggered, clear the histogram data
      if ( reset )
      {
        fall_count_total = 0;
        fall_count_max = 0;
        for (int p = 0; p < peg_row - 1; p++)
        {
          fall_count[p] = 0;
          histogram_height[p] = 0;
        }
      }
      // Scale the 12-bit ADC value (0-4095) to a 5-bit value (1-32) for parameter control
      adc_value_32 = ( adc_value_raw >> 7) + 1;

      // --- USER INPUT: BUTTON ---
      // Read the state of the GPIO button
      button_not_pushed = gpio_get(BUTTON_PIN);
      // On a falling edge (button push), cycle to the next control state
      if ( !button_not_pushed && button_not_pushed_prev ) {
        ctrl_state = (ctrl_state + 1) % ctrl_state_num;
      }
      button_not_pushed_prev = button_not_pushed;

      // --- UPDATE PARAMETERS BASED ON CONTROL STATE ---
      // Use the ADC value to adjust the currently selected parameter
      if ( ctrl_state == CTRL_BALL_NUM )
      {
        // Scale ADC value to control the total number of balls
        ball_num_total = ( (ball_num_max * adc_value_raw) >> 15 ) * 8;
      } else if ( ctrl_state == CTRL_BOUNCINESS ) {
        // Scale ADC value to control bounciness (0.0 to 1.0)
        bounciness = ( int2fix15(adc_value_32) >> 5 );
        bounciness_float = fix2float15(bounciness);
      } else if ( ctrl_state == CTRL_GRAVITY ) {
        // Scale ADC value to control gravity (0.0 to 1.0)
        g = ( int2fix15(adc_value_32) >> 5 );
        g_float = fix2float15(g);
      }

      // Update the number of balls for this core
      ball_num0_prev = ball_num0;
      ball_num0 = ball_num_total / 2;

      // Store previous histogram height for efficient erasing
      for (int i = 0; i < peg_row - 1; i++)
      {
        histogram_height_prev0[i] = histogram_height[i];
      }

      // --- RENDERING: ERASE OLD POSITIONS ---
      // Erase balls from their previous positions
      for (int i = 0; i < ball_num0_prev; i++)
      {
        fillCircle(fix2int15(ball0_x[i]), fix2int15(ball0_y[i]), ball_r_int, BLACK);
      }

      // --- PHYSICS UPDATE ---
      // Update ball positions and velocities
      moveBall0();

      // --- RENDERING: DRAW NEW POSITIONS ---
      // Draw the balls at their new positions
      for (int i = 0; i < ball_num0; i++)
      {
        fillCircle(fix2int15(ball0_x[i]), fix2int15(ball0_y[i]), ball_r_int, ball_color_0);
      }

      // --- RENDERING: TEXT DISPLAY ---
      // Clear the text area and display updated simulation info
      fillRect(0, 0, 180, 70, BLACK);
      sprintf(text_line1, "Time: %d s", time_us_32()/1000000);
      sprintf(text_line2, "Re-spawn count: %d", fall_count_total);
      sprintf(text_line3, "Current number of balls: %d", ball_num_total);
      sprintf(text_line4, "Bounciness: %.2f", bounciness_float);
      sprintf(text_line5, "Gravity: %.2f", g_float);
      if ( ctrl_state == CTRL_NONE ) {
        sprintf(text_line6, "Control: None");
      } else if ( ctrl_state == CTRL_BALL_NUM ) {
        sprintf(text_line6, "Control: Number of balls");
      } else if ( ctrl_state == CTRL_BOUNCINESS ) {
        sprintf(text_line6, "Control: Bounciness");
      } else if ( ctrl_state == CTRL_GRAVITY ) {
        sprintf(text_line6, "Control: Gravity");
      }
      setCursor(10, 10);
      writeString(text_line1);
      setCursor(10, 20);
      writeString(text_line2);
      setCursor(10, 30);
      writeString(text_line3);
      setCursor(10, 40);
      writeString(text_line4);
      setCursor(10, 50);
      writeString(text_line5);
      setCursor(10, 60);
      writeString(text_line6);

      // --- RENDERING: HISTOGRAM ---
      // If parameters were reset, clear the whole histogram area
      if ( reset ) {
        fillRect(histogram_start_x - 1, screen_height - histogram_height_max, 
                 (peg_row - 1)*histogram_width + 2, histogram_height_max + 2, BLACK);
      }
      // Redraw the histogram bars
      for ( int i = 0; i < peg_row - 1; i++ )
      {
        // Erase the previous bar for this bin
        drawRect( histogram_start_x + i*histogram_width, screen_height - histogram_height_prev0[i], 
                  histogram_width - 2, histogram_height_prev0[i], BLACK ) ;

        // Draw the new, updated bar
        drawRect( histogram_start_x + i*histogram_width, screen_height - histogram_height[i], 
                  histogram_width - 2, histogram_height[i], histogram_color ) ;
      }

      // --- FRAME RATE CONTROL ---
      // Calculate how much time is left in the current frame
      spare_time = FRAME_RATE - (time_us_32() - begin_time) ;
      // If we missed the deadline, turn on the LED
      if ( spare_time < 0 ) {
        gpio_put(LED_PIN, 1);
      } else {
        gpio_put(LED_PIN, 0);
      }
      // Yield for the remaining time to maintain the target frame rate
      PT_YIELD_usec(spare_time) ;
    } // END WHILE(1)
  PT_END(pt);
} // animation thread

// Animation on core 1
// This is the main animation thread for core 1. It runs in a tight loop
// to handle the physics and rendering for its half of the balls.
// It does not handle user input or text display; that is all done on core 0.
static PT_THREAD (protothread_anim1(struct pt *pt))
{
    // Mark beginning of thread
    PT_BEGIN(pt);

    // Variables for maintaining a consistent frame rate
    static int begin_time; // Timestamp at the start of a frame
    static int spare_time; // Time left over at the end of a frame

    // Main animation loop
    while(1) {
      // --- FRAME TIMING ---
      // Record the start time of the frame
      begin_time = time_us_32();

      // Update the number of balls for this core
      ball_num1_prev = ball_num1;
      ball_num1 = (ball_num_total + 1) / 2;

      // Store previous histogram height for efficient erasing
      for (int i = 0; i < peg_row - 1; i++)
      {
        histogram_height_prev1[i] = histogram_height[i];
      }

      // --- RENDERING: ERASE OLD POSITIONS ---
      // Erase balls from their previous positions
      for (int i = 0; i < ball_num1_prev; i++)
      {
        fillCircle(fix2int15(ball1_x[i]), fix2int15(ball1_y[i]), ball_r_int, BLACK);
      }

      // --- PHYSICS UPDATE ---
      // Update ball positions and velocities
      moveBall1();

      // --- RENDERING: DRAW NEW POSITIONS ---
      // Draw the balls at their new positions
      for (int i = 0; i < ball_num1; i++)
      {
        fillCircle(fix2int15(ball1_x[i]), fix2int15(ball1_y[i]), ball_r_int, ball_color_1);
      }

      // --- RENDERING: HISTOGRAM ---
      // Redraw the histogram bars. Note that both cores draw the histogram,
      // which is redundant but ensures it is drawn completely.
      for ( int i = 0; i < peg_row - 1; i++ )
      {
        // Erase the previous bar for this bin
        drawRect( histogram_start_x + i*histogram_width, screen_height - histogram_height_prev1[i], 
                  histogram_width - 2, histogram_height_prev1[i], BLACK ) ;
        // Draw the new, updated bar
        drawRect( histogram_start_x + i*histogram_width, screen_height - histogram_height[i], 
                  histogram_width - 2, histogram_height[i], histogram_color ) ;
      }

      // --- FRAME RATE CONTROL ---
      // Calculate how much time is left in the current frame
      spare_time = FRAME_RATE - (time_us_32() - begin_time) ;
      // Yield for the remaining time to maintain the target frame rate
      PT_YIELD_usec(spare_time) ;
    } // END WHILE(1)
  PT_END(pt);
} // animation thread

// ========================================
// === core 1 main -- started in main below
// ========================================
void core1_main(){
  // Add animation thread
  pt_add_thread(protothread_anim1);
  // Start the scheduler
  pt_schedule_start ;

}

// ========================================
// === main
// ========================================
// USE ONLY C-sdk library
int main(){

  // ============================================
  // ====== CODE FROM DMA DEMO STARTS HERE ======
  // ============================================

  // Initidalize stdio
  stdio_init_all();

  // Initialize SPI channel (channel, baud rate set to 20MHz)
  spi_init(SPI_PORT, 20000000) ;

  // Format SPI channel (channel, data bits per transfer, polarity, phase, order)
  spi_set_format(SPI_PORT, 16, 0, 0, 0);

  // Map SPI signals to GPIO ports, acts like framed SPI with this CS mapping
  gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
  gpio_set_function(PIN_CS, GPIO_FUNC_SPI) ;
  gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
  gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

  // Build sine table and DAC data table
  int i ;
  for (i=0; i<(sine_table_size); i++){
      raw_sin[i] = (int)(2047 * sin((float)i*6.283/(float)sine_table_size) + 2047); //12 bit
      DAC_data[i] = DAC_config_chan_A | (raw_sin[i] & 0x0fff) ;
  }

  // Select DMA channels (now global)
  data_chan = dma_claim_unused_channel(true);
  ctrl_chan = dma_claim_unused_channel(true);

  // Setup the control channel
  dma_channel_config c = dma_channel_get_default_config(ctrl_chan);   // default configs
  channel_config_set_transfer_data_size(&c, DMA_SIZE_32);             // 32-bit txfers
  channel_config_set_read_increment(&c, false);                       // no read incrementing
  channel_config_set_write_increment(&c, false);                      // no write incrementing
  channel_config_set_chain_to(&c, data_chan);                         // chain to data channel

  dma_channel_configure(
      ctrl_chan,                          // Channel to be configured
      &c,                                 // The configuration we just created
      &dma_hw->ch[data_chan].read_addr,   // Write address (data channel read address)
      &address_pointer_dma,                   // Read address (POINTER TO AN ADDRESS)
      1,                                  // Number of transfers
      false                               // Don't start immediately
  );

  // Setup the data channel
  dma_channel_config c2 = dma_channel_get_default_config(data_chan);  // Default configs
  channel_config_set_transfer_data_size(&c2, DMA_SIZE_16);            // 16-bit txfers
  channel_config_set_read_increment(&c2, true);                       // yes read incrementing
  channel_config_set_write_increment(&c2, false);                     // no write incrementing
  // (X/Y)*sys_clk, where X is the first 16 bytes and Y is the second
  // sys_clk is 125 MHz unless changed in code. Configured to ~44 kHz
  dma_timer_set_fraction(0, 0x0017, 0xffff) ;
  // 0x3b means timer0 (see SDK manual)
  channel_config_set_dreq(&c2, 0x3b);                                 // DREQ paced by timer 0
  // chain to the controller DMA channel
  // channel_config_set_chain_to(&c2, ctrl_chan);                        // Chain to control channel


  dma_channel_configure(
      data_chan,                  // Channel to be configured
      &c2,                        // The configuration we just created
      &spi_get_hw(SPI_PORT)->dr,  // write address (SPI data register)
      DAC_data,                   // The initial read address
      sine_table_size,            // Number of transfers
      false                       // Don't start immediately.
  );


  // start the control channel
  // dma_start_channel_mask(1u << ctrl_chan) ;

  // Exit main.
  // No code executing!!

  // ==========================================
  // ====== CODE FROM DMA DEMO ENDS HERE ======
  // ==========================================

  set_sys_clock_khz(250000, true) ;
  // initialize stdio
  // stdio_init_all() ;

  // initialize VGA
  initVGA() ;

  // initialize ADC
  adc_init();
  adc_gpio_init(ADC_PIN);
  adc_select_input(0);

  // Initialize GPIO button
  gpio_init(BUTTON_PIN);
  gpio_set_dir(BUTTON_PIN, GPIO_IN);
  gpio_pull_up(BUTTON_PIN);

  // Initialize LED pin
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  gpio_put(LED_PIN, 0);

  ball_num0 = ball_num_total / 2;
  ball_num0_prev = ball_num0;
  ball_num1 = ( ball_num_total + 1 ) / 2;
  ball_num1_prev = ball_num1;

  for (int i = 0; i < ball_num_max0; i++)
  {
    ball0_peg_index_prev[i] = -1;
  }
  for (int i = 0; i < ball_num_max1; i++)
  {
    ball1_peg_index_prev[i] = -1;
  }

  // Create peg
  createPeg();

  // Create balls
  createBall0();
  createBall1();

  // Display text settings
  setTextColor(WHITE);
  setTextSize(1);
  
  // Convert parameters
  g = float2fix15(g_float);
  ball_r = int2fix15(ball_r_int);
  peg_r = int2fix15(peg_r_int);
  bounciness = float2fix15(bounciness_float);

  // start core 1 
  multicore_reset_core1();
  multicore_launch_core1(&core1_main);

  // add threads
  // pt_add_thread(protothread_serial);
  pt_add_thread(protothread_anim0);

  // start scheduler
  pt_schedule_start ;
}