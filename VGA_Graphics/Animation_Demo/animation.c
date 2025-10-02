/**
 * Hunter Adams (vha3@cornell.edu)
 * 
 * This demonstration animates two balls bouncing about the screen.
 * Through a serial interface, the user can change the ball color.
 *
 * HARDWARE CONNECTIONS
  - GPIO 16 ---> Pin 21 ---> VGA Hsync
  - GPIO 17 ---> Pin 22 ---> VGA Vsync
  - GPIO 18 ---> Pin 24 ---> VGA Green lo-bit --> 470 ohm resistor --> VGA_Green
  - GPIO 19 ---> Pin 25 ---> VGA Green hi_bit --> 330 ohm resistor --> VGA_Green
  - GPIO 20 ---> Pin 26 ---> 330 ohm resistor ---> VGA-Blue
  - GPIO 21 ---> Pin 27 ---> 330 ohm resistor ---> VGA-Red
  - RP2040 GND ---> Pin 23 ---> VGA-GND
 *
 * RESOURCES USED
 *  - PIO state machines 0, 1, and 2 on PIO instance 0
 *  - DMA channels (2, by claim mechanism)
 *  - 153.6 kBytes of RAM (for pixel color data)
 *
 */

// Include the VGA grahics library
#include "vga16_graphics_v2.h"
// Include standard libraries
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
// Include Pico libraries
#include "pico/stdlib.h"
#include "pico/divider.h"
#include "pico/multicore.h"
// Include hardware libraries
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/spi.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
// Include protothreads
#include "pt_cornell_rp2040_v1_4.h"

// Default values
int ball_num_total = 128;          // Total number of balls (initial value)
float bounciness_float = 0.38;   //initial value
float g_float = 0.75;            //initial value

// ADC
#define ADC_PIN 26
int adc_value_raw;
int adc_value_32;
int adc_value_raw_history[10] = {0};

bool reset = false;

// Control state
#define CTRL_NONE        0
#define CTRL_BALL_NUM    1
#define CTRL_BOUNCINESS  2
#define CTRL_GRAVITY     3
volatile int ctrl_state = CTRL_NONE;
int ctrl_state_num = 4;

// GPIO button
#define BUTTON_PIN  2
volatile bool button_not_pushed = true;
volatile bool button_not_pushed_prev = true;

// LED pin
#define LED_PIN 25

// ============================================
// ====== CODE FROM DMA DEMO STARTS HERE ======
// ============================================

// Number of samples per period in sine table
#define sine_table_size 256

// Sine table
int raw_sin[sine_table_size] ;

// Table of values to be sent to DAC
unsigned short DAC_data[sine_table_size] ;

// Pointer to the address of the DAC data table
unsigned short * address_pointer_dma = &DAC_data[0] ;

// DMA channel variables
int data_chan;
int ctrl_chan;

// A-channel, 1x, active
#define DAC_config_chan_A 0b0011000000000000

//SPI configurations
#define PIN_MISO 4
#define PIN_CS   5
#define PIN_SCK  6
#define PIN_MOSI 7
#define SPI_PORT spi0

// Number of DMA transfers per event
const uint32_t transfer_count = sine_table_size ;

// ==========================================
// ====== CODE FROM DMA DEMO ENDS HERE ======
// ==========================================

// === the fixed point macros ========================================
typedef signed int fix15 ;
#define multfix15(a,b) ((fix15)((((signed long long)(a))*((signed long long)(b)))>>15))
#define float2fix15(a) ((fix15)((a)*32768.0)) // 2^15
#define fix2float15(a) ((float)(a)/32768.0)
#define absfix15(a) abs(a) 
#define int2fix15(a) ((fix15)(a << 15))
#define fix2int15(a) ((int)(a >> 15))
#define char2fix15(a) (fix15)(((fix15)(a)) << 15)
#define divfix(a,b) (fix15)(div_s64s64( (((signed long long)(a)) << 15), ((signed long long)(b))))
#define sqrtfix(a) (float2fix15(sqrt(fix2float15(a))))

// uS per frame
#define FRAME_RATE 33000

// Screen dimensions
#define screen_width   640
#define screen_height  480

// Colors
char ball_color_0 =    YELLOW;
char ball_color_1 =    BLUE;
char peg_color =       GREEN;
char histogram_color = WHITE;

// Physics parameters
fix15 g;
fix15 bounciness;

// Ball parameters
#define ball_num_max 800                    // Maximum number of balls
#define ball_num_max0 (ball_num_max/2)      // Maximum number of balls on core 0
#define ball_num_max1 ((ball_num_max+1)/2)  // Maximum number of balls on core 1
int ball_r_int = 4;   // Ball radius
fix15 ball_r;

// Number of balls
int ball_num0;                      // Number of balls on core 0 = ball_num_total / 2
int ball_num0_prev;                 // Previous number of balls on core 0
int ball_num1;                      // Number of balls on core 1 = ( ball_num_total + 1 ) / 2
int ball_num1_prev;                 // Previous number of balls on core 1

// Ball on core 0
fix15 ball0_x[ball_num_max0];   // Ball position x on core 0
fix15 ball0_y[ball_num_max0];   // Ball position y on core 0
fix15 ball0_vx[ball_num_max0];  // Ball velocity x on core 0
fix15 ball0_vy[ball_num_max0];  // Ball velocity y on core 0
int ball0_peg_index_prev[ball_num_max0]; // Previous peg index of last collision for each ball on core 0

// Ball on core 1
fix15 ball1_x[ball_num_max1];   // Ball position x on core 1
fix15 ball1_y[ball_num_max1];   // Ball position y on core 1
fix15 ball1_vx[ball_num_max1];  // Ball velocity x on core 1
fix15 ball1_vy[ball_num_max1];  // Ball velocity y on core 1
int ball1_peg_index_prev[ball_num_max1]; // Previous peg index of last collision for each ball on core 1

// Peg parameters
#define peg_row        16   // Number of rows of pegs
#define peg_num        ((peg_row*(peg_row+1))/2)  // Total number of pegs
#define peg_start_x    screen_width/2             // Top peg position
#define peg_start_y    40   // Top peg position
#define peg_space_x    38   // Horizontal spacing between pegs
#define peg_space_y    19   // Vertical spacing between pegs
int peg_r_int = 6;   // Peg radius
fix15 peg_r;

// Peg position
int peg_x_int[peg_num];   // Peg position x
int peg_y_int[peg_num];   // Peg position y
fix15 peg_x[peg_num];
fix15 peg_y[peg_num];

// Text display
char text_line1[32];
char text_line2[32];
char text_line3[32];
char text_line4[32];
char text_line5[32];
char text_line6[32];

// Fall count
int fall_count_total = 0;
int fall_count[peg_row - 1] = {0};
int fall_count_max = 0;

// Histogram parameters
#define histogram_height_max   100
#define histogram_width        peg_space_x
#define histogram_start_x      (screen_width/2 - ((peg_row - 1)*histogram_width/2))
int histogram_height[peg_row - 1] = {0};
int histogram_height_prev0[peg_row - 1] = {0};
int histogram_height_prev1[peg_row - 1] = {0};

// Create pegs
void createPeg()
{
  int peg_index = 0;
  for (int r = 1; r <= peg_row; r++)
  {
    for (int i = 1; i <= r; i++)
    {
      peg_x_int[peg_index] = peg_start_x - peg_space_x*(r-1)/2 + peg_space_x*(i-1);
      peg_y_int[peg_index] = peg_start_y + peg_space_y*(r-1);
      peg_index += 1;
    }
  }

  for (int i = 0; i < peg_num; i++)
  {
    peg_x[i] = int2fix15(peg_x_int[i]);
    peg_y[i] = int2fix15(peg_y_int[i]);
  }
}

// Create balls on core 0
void createBall0()
{
  // Start in center top
  for (int i = 0; i < ball_num_max0; i++)
  {
    ball0_x[i] = int2fix15(screen_width/2);
    ball0_y[i] = int2fix15(ball_r_int);
    ball0_vx[i] = (fix15)((rand() & 0xffff) - int2fix15(1));
    ball0_vy[i] = int2fix15(0);
  }
}

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

// Update ball position and velocity on core 0
static inline void moveBall0()
{
  for (int i = 0; i < ball_num0; i++)
  {
    for (int j = 0; j < peg_num; j++)
    {
      // Peg collision
      fix15 dx = ball0_x[i] - peg_x[j];
      fix15 dy = ball0_y[i] - peg_y[j];
      if ( (abs(dx) < ball_r + peg_r) && (abs(dy) < ball_r + peg_r) )
      {
        // fix15 distance = sqrtfix(multfix15(dx, dx) + multfix15(dy, dy));
        fix15 max;
        fix15 min;
        if (absfix15(dx) > absfix15(dy)) {
          max = absfix15(dx);
          min = absfix15(dy);
        } else {
          max = absfix15(dy);
          min = absfix15(dx);
        }
        fix15 distance = max + (min >> 2);   // Approximation of sqrt(dx*dx + dy*dy)

        fix15 normal_x = divfix(dx, distance);
        fix15 normal_y = divfix(dy, distance);

        fix15 intermediate_term = multfix15(int2fix15(-2), 
          (multfix15(normal_x,  ball0_vx[i]) + multfix15(normal_y, ball0_vy[i])));

        if (intermediate_term > int2fix15(0))
        {
          //ball.x = peg.x + (normal_x * (distance+1))
          ball0_x[i] = peg_x[j] + multfix15(normal_x, (ball_r + peg_r + int2fix15(1)));
          ball0_y[i] = peg_y[j] + multfix15(normal_y, (ball_r + peg_r + int2fix15(1)));
          //ball.vx = ball.vx + (normal_x * intermediate_term)
            ball0_vx[i] = ball0_vx[i] + multfix15(normal_x, intermediate_term);
            ball0_vy[i] = ball0_vy[i] + multfix15(normal_y, intermediate_term);
          if ( j != ball0_peg_index_prev[i] )
          {
            // New peg collision
            ball0_peg_index_prev[i] = j;

            ball0_vx[i] = multfix15(ball0_vx[i], bounciness);
            ball0_vy[i] = multfix15(ball0_vy[i], bounciness);

            // Trigger DMA on collision, if channel is not already busy
            if (!dma_channel_is_busy(data_chan)) {
              dma_start_channel_mask(1u << ctrl_chan);
            }

            // Only handle one collision per ball per frame
            break;
          }
        }
      }
    }

    // Hit walls
    if ( (fix2int15(ball0_x[i]) < ball_r_int) || (fix2int15(ball0_x[i]) > (screen_width - ball_r_int)) )
    {
      ball0_vx[i] = -ball0_vx[i];
    }
    if ( fix2int15(ball0_y[i]) < ball_r_int )
    {
      ball0_vy[i] = -ball0_vy[i];
    }

    // Ball re-spawn
    if ( fix2int15(ball0_y[i]) > (screen_height - histogram_height_max - 40) )
    {
      // Update fall count
      fall_count_total += 1;
      for (int p = 0; p < peg_row - 1; p++)
      {
        if ( (fix2int15(ball0_x[i]) >= (histogram_start_x + p*histogram_width)) && 
             (fix2int15(ball0_x[i]) < (histogram_start_x + (p+1)*histogram_width)) )
        {
          fall_count[p] += 1;
          if ( fall_count[p] > fall_count_max )
          {
            fall_count_max = fall_count[p];
          }
          break;
        }
      }

      for (int p = 0; p < peg_row - 1; p++)
      {
        histogram_height[p] = (fall_count[p] * histogram_height_max) / fall_count_max;
      }

      // ball0_x[i] = int2fix15(screen_width/2);
      // ball0_y[i] = int2fix15(ball_r_int);
      // ball0_vx[i] = (fix15)((rand() & 0xffff) - int2fix15(1));
      // ball0_vy[i] = int2fix15(0);
      ball0_x[i] = int2fix15(screen_width);
      ball0_y[i] = int2fix15(screen_height/2 + 50);
      ball0_vx[i] = (fix15)((rand() & 0xffff) - int2fix15(10));
      ball0_vy[i] = (fix15)((rand() & 0xffff) - int2fix15(20));
    }

    // Gravity
    ball0_vy[i] = ball0_vy[i] + g;

    // Update position
    ball0_x[i] = ball0_x[i] + ball0_vx[i];
    ball0_y[i] = ball0_y[i] + ball0_vy[i];
  }
}

// Update ball position and velocity on core 1
static inline void moveBall1()
{
  for (int i = 0; i < ball_num1; i++)
  {
    for (int j = 0; j < peg_num; j++)
    {
      // Peg collision
      fix15 dx = ball1_x[i] - peg_x[j];
      fix15 dy = ball1_y[i] - peg_y[j];
      if ( (abs(dx) < ball_r + peg_r) && (abs(dy) < ball_r + peg_r) )
      {
        // fix15 distance = sqrtfix(multfix15(dx, dx) + multfix15(dy, dy));
        fix15 max;
        fix15 min;
        if (absfix15(dx) > absfix15(dy)) {
          max = absfix15(dx);
          min = absfix15(dy);
        } else {
          max = absfix15(dy);
          min = absfix15(dx);
        }
        fix15 distance = max + (min >> 2);   // Approximation of sqrt(dx*dx + dy*dy)

        fix15 normal_x = divfix(dx, distance);
        fix15 normal_y = divfix(dy, distance);

        fix15 intermediate_term = multfix15(int2fix15(-2), 
          (multfix15(normal_x,  ball1_vx[i]) + multfix15(normal_y, ball1_vy[i])));

        if (intermediate_term > int2fix15(0))
        {
          //ball.x = peg.x + (normal_x * (distance+1))
          ball1_x[i] = peg_x[j] + multfix15(normal_x, (ball_r + peg_r + int2fix15(1)));
          ball1_y[i] = peg_y[j] + multfix15(normal_y, (ball_r + peg_r + int2fix15(1)));
          //ball.vx = ball.vx + (normal_x * intermediate_term)
          ball1_vx[i] = ball1_vx[i] + multfix15(normal_x, intermediate_term);
          ball1_vy[i] = ball1_vy[i] + multfix15(normal_y, intermediate_term);
          if ( j != ball1_peg_index_prev[i] )
          {
            // New peg collision
            ball1_peg_index_prev[i] = j;

            ball1_vx[i] = multfix15(ball1_vx[i], bounciness);
            ball1_vy[i] = multfix15(ball1_vy[i], bounciness);

            // Trigger DMA on collision, if channel is not already busy
            if (!dma_channel_is_busy(data_chan)) {
              dma_start_channel_mask(1u << ctrl_chan);
            }

            // Only handle one collision per ball per frame
            break;
          }
        }
      }
    }

    // Hit walls
    if ( (fix2int15(ball1_x[i]) < ball_r_int) || (fix2int15(ball1_x[i]) > (screen_width - ball_r_int)) )
    {
      ball1_vx[i] = -ball1_vx[i];
    }
    if ( fix2int15(ball1_y[i]) < ball_r_int )
    {
      ball1_vy[i] = -ball1_vy[i];
    }

    // Ball re-spawn
    if ( fix2int15(ball1_y[i]) > (screen_height - histogram_height_max - 40) )
    {
      // Update fall count
      fall_count_total += 1;
      for (int p = 0; p < peg_row - 1; p++)
      {
        if ( (fix2int15(ball1_x[i]) >= (histogram_start_x + p*histogram_width)) && 
             (fix2int15(ball1_x[i]) < (histogram_start_x + (p+1)*histogram_width)) )
        {
          fall_count[p] += 1;
          if ( fall_count[p] > fall_count_max )
          {
            fall_count_max = fall_count[p];
          }
          break;
        }
      }

      for (int p = 0; p < peg_row - 1; p++)
      {
        histogram_height[p] = (fall_count[p] * histogram_height_max) / fall_count_max;
      }

      // ball1_x[i] = int2fix15(screen_width/2);
      // ball1_y[i] = int2fix15(ball_r_int);
      // ball1_vx[i] = (fix15)((rand() & 0xffff) - int2fix15(1));
      // ball1_vy[i] = int2fix15(0);
      ball1_x[i] = int2fix15(0);
      ball1_y[i] = int2fix15(screen_height/2 + 50);
      ball1_vx[i] = (fix15)((rand() & 0xffff) + int2fix15(10));
      ball1_vy[i] = (fix15)((rand() & 0xffff) - int2fix15(20));
    }

    // Gravity
    ball1_vy[i] = ball1_vy[i] + g;

    // Update position
    ball1_x[i] = ball1_x[i] + ball1_vx[i];
    ball1_y[i] = ball1_y[i] + ball1_vy[i];
  }
}

// ==================================================
// === users serial input thread
// ==================================================
static PT_THREAD (protothread_serial(struct pt *pt))
{
    PT_BEGIN(pt);
    // stores user input
    static int user_input ;
    // wait for 0.1 sec
    PT_YIELD_usec(1000000) ;
    // announce the threader version
    sprintf(pt_serial_out_buffer, "Protothreads RP2040 v1.0\n\r");
    // non-blocking write
    serial_write ;
      while(1) {
        // print prompt
        sprintf(pt_serial_out_buffer, "input a number in the range 1-15: ");
        // non-blocking write
        serial_write ;
        // spawn a thread to do the non-blocking serial read
        serial_read ;
        // convert input string to number
        sscanf(pt_serial_in_buffer,"%d", &user_input) ;
        // update boid color
        if ((user_input > 0) && (user_input < 16)) {
          ball_color_0 = (char)user_input ;
        }
      } // END WHILE(1)
  PT_END(pt);
} // timer thread

// Animation on core 0
static PT_THREAD (protothread_anim0(struct pt *pt))
{
    // Mark beginning of thread
    PT_BEGIN(pt);

    // Variables for maintaining frame rate
    static int begin_time ;
    static int spare_time ;

    while(1) {
      // Measure time at start of thread
      begin_time = time_us_32();

      // ADC read
      adc_value_raw = adc_read();
      for ( int i = 0; i < 9; i++)
      {
        adc_value_raw_history[i] = adc_value_raw_history[i+1];
      }
      adc_value_raw_history[9] = adc_value_raw;
      if ( abs(adc_value_raw - adc_value_raw_history[0]) > 200 )
      {
        if ( ctrl_state != CTRL_NONE )
        {
          reset = true;
        }
      } else 
      {
        reset = false;
      }
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
      // adc_value = adc_value_raw;
      adc_value_32 = ( adc_value_raw >> 7) + 1;  // Scale to 5 bits (1 to 32)

      // GPIO button read
      button_not_pushed = gpio_get(BUTTON_PIN);
      if ( !button_not_pushed && button_not_pushed_prev ) {
        // Button was just pushed
        ctrl_state = (ctrl_state + 1) % ctrl_state_num;
      }
      button_not_pushed_prev = button_not_pushed;

      if ( ctrl_state == CTRL_BALL_NUM )
      {
        ball_num_total = ( (ball_num_max * adc_value_raw) >> 15 ) * 8;  // Scale to 0 to ball_num_max
      } else if ( ctrl_state == CTRL_BOUNCINESS ) {
        bounciness = ( int2fix15(adc_value_32) >> 5 );  // Scale to 0 to 1;
        bounciness_float = fix2float15(bounciness);
      } else if ( ctrl_state == CTRL_GRAVITY ) {
        g = ( int2fix15(adc_value_32) >> 5 );  // Scale to 0 to 1;
        g_float = fix2float15(g);
      }

      ball_num0_prev = ball_num0;
      ball_num0 = ball_num_total / 2;

      // Store previous histogram height
      for (int i = 0; i < peg_row - 1; i++)
      {
        histogram_height_prev0[i] = histogram_height[i];
      }

      // Erase ball
      for (int i = 0; i < ball_num0_prev; i++)
      {
        fillCircle(fix2int15(ball0_x[i]), fix2int15(ball0_y[i]), ball_r_int, BLACK);
      }

      // Update ball position and velocity
      moveBall0();

      // Draw the ball at new position
      for (int i = 0; i < ball_num0; i++)
      {
        fillCircle(fix2int15(ball0_x[i]), fix2int15(ball0_y[i]), ball_r_int, ball_color_0);
      }

      // Draw pegs
      for(int i = 0; i < peg_num; i++)
      {
        fillCircle(peg_x_int[i], peg_y_int[i], peg_r_int, peg_color);
      }

      // Display text
      fillRect(0, 0, 180, 70, BLACK);  // Clear previous text
      sprintf(text_line1, "Time: %d s", time_us_32()/1000000);
      sprintf(text_line2, "Re-spawn count: %d", fall_count_total);
      sprintf(text_line3, "Current number of balls: %d", ball_num_total);
      sprintf(text_line4, "Bounciness: %.2f", bounciness_float);
      sprintf(text_line5, "Gravity: %.2f", g_float);
      if ( ctrl_state == CTRL_NONE )
      {
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

      // Display histogram
      if ( reset ) {
        fillRect(histogram_start_x - 1, screen_height - histogram_height_max, 
                 (peg_row - 1)*histogram_width + 2, histogram_height_max + 2, BLACK);
      }
      for (int i = 0; i < peg_row - 1; i++)
      {
        if (histogram_height[i] > histogram_height_prev0[i]) {
          // Increase in height
          fillRect(histogram_start_x + i*histogram_width, screen_height - histogram_height[i], 
                   histogram_width - 2, histogram_height[i] - histogram_height_prev0[i], histogram_color);
        }
        // Fill the top in black
        fillRect(histogram_start_x + i*histogram_width, screen_height - histogram_height_max, 
                 histogram_width - 2, histogram_height_max - histogram_height[i], BLACK);
        histogram_height_prev0[i] = histogram_height[i];
      }

      // delay in accordance with frame rate
      spare_time = FRAME_RATE - (time_us_32() - begin_time) ;
      // Set LED pin if spare time is negative
      if ( spare_time < 0 ) {
        gpio_put(LED_PIN, 1);
      } else {
        gpio_put(LED_PIN, 0);
      }
      // yield for necessary amount of time
      PT_YIELD_usec(spare_time) ;
     // NEVER exit while
    } // END WHILE(1)
  PT_END(pt);
} // animation thread


// Animation on core 1
static PT_THREAD (protothread_anim1(struct pt *pt))
{
    // Mark beginning of thread
    PT_BEGIN(pt);

    // Variables for maintaining frame rate
    static int begin_time ;
    static int spare_time ;

    while(1) {
      // Measure time at start of thread
      begin_time = time_us_32();

      // ADC read
      adc_value_raw = adc_read();
      for ( int i = 0; i < 9; i++)
      {
        adc_value_raw_history[i] = adc_value_raw_history[i+1];
      }
      adc_value_raw_history[9] = adc_value_raw;
      if ( abs(adc_value_raw - adc_value_raw_history[0]) > 200 )
      {
        if ( ctrl_state != CTRL_NONE )
        {
          reset = true;
        }
      } else 
      {
        reset = false;
      }
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
      adc_value_32 = ( adc_value_raw >> 7) + 1;  // Scale to 5 bits (1 to 32)

      // GPIO button read
      button_not_pushed = gpio_get(BUTTON_PIN);
      if ( !button_not_pushed && button_not_pushed_prev ) {
        // Button was just pushed
        ctrl_state = (ctrl_state + 1) % ctrl_state_num;
      }
      button_not_pushed_prev = button_not_pushed;

      if ( ctrl_state == CTRL_BALL_NUM )
      {
        ball_num_total = ( (ball_num_max * adc_value_raw) >> 15 ) * 8;  // Scale to 0 to ball_num_max
      } else if ( ctrl_state == CTRL_BOUNCINESS ) {
        bounciness = ( int2fix15(adc_value_32) >> 5 );  // Scale to 0 to 1;
        bounciness_float = fix2float15(bounciness);
      } else if ( ctrl_state == CTRL_GRAVITY ) {
        g = ( int2fix15(adc_value_32) >> 5 );  // Scale to 0 to 1;
        g_float = fix2float15(g);
      }

      ball_num1_prev = ball_num1;
      ball_num1 = ( ball_num_total + 1 ) / 2;

      // Store previous histogram height
      for (int i = 0; i < peg_row - 1; i++)
      {
        histogram_height_prev1[i] = histogram_height[i];
      }

      // Erase ball
      for (int i = 0; i < ball_num1_prev; i++)
      {
        fillCircle(fix2int15(ball1_x[i]), fix2int15(ball1_y[i]), ball_r_int, BLACK);
      }

      // Update ball position and velocity
      moveBall1();

      // Draw the ball at new position
      for (int i = 0; i < ball_num1; i++)
      {
        fillCircle(fix2int15(ball1_x[i]), fix2int15(ball1_y[i]), ball_r_int, ball_color_1);
      }

      // Draw pegs
      for(int i = 0; i < peg_num; i++)
      {
        fillCircle(peg_x_int[i], peg_y_int[i], peg_r_int, peg_color);
      }

      // Display histogram
      if ( reset ) {
        fillRect(histogram_start_x - 1, screen_height - histogram_height_max, 
                 (peg_row - 1)*histogram_width + 2, histogram_height_max + 2, BLACK);
      }
      for (int i = 0; i < peg_row - 1; i++)
      {
        if (histogram_height[i] > histogram_height_prev1[i]) {
          // Increase in height
          fillRect(histogram_start_x + i*histogram_width, screen_height - histogram_height[i], 
                   histogram_width - 2, histogram_height[i] - histogram_height_prev1[i], histogram_color);
        }
        // Fill the top in black
        fillRect(histogram_start_x + i*histogram_width, screen_height - histogram_height_max, 
                 histogram_width - 2, histogram_height_max - histogram_height[i], BLACK);
        histogram_height_prev1[i] = histogram_height[i];
      }

      // delay in accordance with frame rate
      spare_time = FRAME_RATE - (time_us_32() - begin_time) ;
      // Set LED pin if spare time is negative
      if ( spare_time < 0 ) {
        gpio_put(LED_PIN, 1);
      } else {
        gpio_put(LED_PIN, 0);
      }
      // yield for necessary amount of time
      PT_YIELD_usec(spare_time) ;
     // NEVER exit while
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

  set_sys_clock_khz(150000, true) ;
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