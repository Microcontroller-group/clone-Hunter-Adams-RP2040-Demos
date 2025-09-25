
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
// Include protothreads
#include "pt_cornell_rp2040_v1_4.h"

// ==================================================
// === CODE FROM DMA DEMO STARTS HERE =================
// ==================================================

// Number of samples per period in sine table
#define sine_table_size 256

// Sine table
int raw_sin[sine_table_size] ;

// Table of values to be sent to DAC
unsigned short DAC_data[sine_table_size] ;

// Pointer to the address of the DAC data table
unsigned short * address_pointer_dma = &DAC_data[0] ;

// DMA channel variables (now global for collision-triggered DMA)
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

// ==================================================
// === CODE FROM DMA DEMO ENDS HERE ===================
// ==================================================

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

// Wall detection
#define hitBottom(b) (b>int2fix15(380))
#define hitTop(b) (b<int2fix15(100))
#define hitLeft(a) (a<int2fix15(100))
#define hitRight(a) (a>int2fix15(540))

// uS per frame
#define FRAME_RATE 33000

// the color of the ball and peg
char ball_color_0 = WHITE ;
char ball_color_1 = BLUE ;
char peg_color = GREEN ;

// Physics parameter
float g_float = 0.75;
fix15 g;
float bounciness_float = 0.8;
fix15 bounciness;

// Ball and peg parameters
#define ball_num  1
#define peg_num   3
#define peg_start_x  320
#define peg_start_y   30
int ball_r_int =  4;
int peg_r_int =   6;
fix15 ball_r;
fix15 peg_r;

// Peg position
int peg_x_int[peg_num];
int peg_y_int[peg_num];
fix15 peg_x[peg_num];
fix15 peg_y[peg_num];

// Create peg
void createPeg()
{
  peg_x_int[0] = peg_start_x;
  peg_y_int[0] = peg_start_y;
  peg_x_int[1] = peg_start_x - 19;
  peg_y_int[1] = peg_start_y + 19;
  peg_x_int[2] = peg_start_x + 19;
  peg_y_int[2] = peg_start_y + 19;

  for (int i = 0; i < peg_num; i++)
  {
    peg_x[i] = int2fix15(peg_x_int[i]);
    peg_y[i] = int2fix15(peg_y_int[i]);
  }
}

// Ball on core 0
fix15 ball0_x[ball_num];
fix15 ball0_y[ball_num];
fix15 ball0_vx[ball_num];
fix15 ball0_vy[ball_num];

// Ball on core 1
fix15 ball1_x[ball_num];
fix15 ball1_y[ball_num];
fix15 ball1_vx[ball_num];
fix15 ball1_vy[ball_num];

// Create ball
void createBall0()
{
  // Start in center top
  for (int i = 0; i < ball_num; i++)
  {
    ball0_x[i] = int2fix15(320);
    ball0_y[i] = int2fix15(0);
    ball0_vx[i] = ((fix15)(rand() & 0xffff) >> 1) - 16384;
    ball0_vy[i] = int2fix15(0);
  }
}

void createBall1()
{
  // Start in center top
  for (int i = 0; i < ball_num; i++)
  {
    ball1_x[i] = int2fix15(320);
    ball1_y[i] = int2fix15(0);
    ball1_vx[i] = ((fix15)(rand() & 0xffff) >> 1) - 16384;
    ball1_vy[i] = int2fix15(0);
  }
}

// Update ball position and velocity
static inline void moveBall0()
{
  for (int i = 0; i < ball_num; i++)
  {
    for (int j = 0; j < peg_num; j++)
    {
      // Peg collision
      fix15 dx = ball0_x[i] - peg_x[j];
      fix15 dy = ball0_y[i] - peg_y[j];
      if ( (abs(dx) < ball_r + peg_r) && (abs(dy) < ball_r + peg_r) )
      {
        fix15 distance = sqrtfix(multfix15(dx, dx) + multfix15(dy, dy));

        fix15 normal_x = divfix(dx, distance);
        fix15 normal_y = divfix(dy, distance);

        fix15 intermediate_term = multfix15(int2fix15(-2), (multfix15(normal_x,  ball0_vx[i]) + multfix15(normal_y, ball0_vy[i])));

        if ( intermediate_term > int2fix15(0))
        {
          //ball.x = peg.x + (normal_x * (distance+1))
          ball0_x[i] = peg_x[j] + multfix15(normal_x, (distance + int2fix15(1)));
          ball0_y[i] = peg_y[j] + multfix15(normal_y, (distance + int2fix15(1)));
          //ball.vx = ball.vx + (normal_x * intermediate_term)
          ball0_vx[i] = ball0_vx[i] + multfix15(normal_x, intermediate_term);
          ball0_vy[i] = ball0_vy[i] + multfix15(normal_y, intermediate_term);
          ball0_vx[i] = multfix15(ball0_vx[i], bounciness);
          ball0_vy[i] = multfix15(ball0_vy[i], bounciness);

          // Trigger DMA on collision
          dma_start_channel_mask(1u << ctrl_chan);
        }
      }
    }
    // Hit walls
    if ( (fix2int15(ball0_x[i]) < 0) || (fix2int15(ball0_x[i]) > 640) )
    {
      ball0_vx[i] = -ball0_vx[i];
    }
    if ( fix2int15(ball0_y[i]) < 0 )
    {
      ball0_vy[i] = -ball0_vy[i];
    }

    // Ball reborn
    if ( fix2int15(ball0_y[i]) > 470 )
    {
      ball0_x[i] = int2fix15(320);
      ball0_y[i] = int2fix15(0);
      ball0_vx[i] = ((fix15)(rand() & 0xffff) >> 1) - 16384;
      ball0_vy[i] = int2fix15(0);
    }

    // Gravity
    ball0_vy[i] = ball0_vy[i] + g;

    // Update position
    ball0_x[i] = ball0_x[i] + ball0_vx[i];
    ball0_y[i] = ball0_y[i] + ball0_vy[i];
  }
}

static inline void moveBall1()
{
  for (int i = 0; i < ball_num; i++)
  {
    for (int j = 0; j < peg_num; j++)
    {
      // Peg collision
      fix15 dx = ball1_x[i] - peg_x[j];
      fix15 dy = ball1_y[i] - peg_y[j];
      if ( (abs(dx) < ball_r + peg_r) && (abs(dy) < ball_r + peg_r) )
      {
        fix15 distance = sqrtfix(multfix15(dx, dx) + multfix15(dy, dy));

        fix15 normal_x = divfix(dx, distance);
        fix15 normal_y = divfix(dy, distance);

        fix15 intermediate_term = multfix15(int2fix15(-2), (multfix15(normal_x,  ball1_vx[i]) + multfix15(normal_y, ball1_vy[i])));

        if ( intermediate_term > int2fix15(0))
        {
          //ball.x = peg.x + (normal_x * (distance+1))
          ball1_x[i] = peg_x[j] + multfix15(normal_x, (distance + int2fix15(1)));
          ball1_y[i] = peg_y[j] + multfix15(normal_y, (distance + int2fix15(1)));
          //ball.vx = ball.vx + (normal_x * intermediate_term)
          ball1_vx[i] = ball1_vx[i] + multfix15(normal_x, intermediate_term);
          ball1_vy[i] = ball1_vy[i] + multfix15(normal_y, intermediate_term);
          ball1_vx[i] = multfix15(ball1_vx[i], bounciness);
          ball1_vy[i] = multfix15(ball1_vy[i], bounciness);

          // Trigger DMA on collision
          dma_start_channel_mask(1u << ctrl_chan);
        }
      }
    }
    // Hit walls
    if ( (fix2int15(ball1_x[i]) < 0) || (fix2int15(ball1_x[i]) > 640) )
    {
      ball1_vx[i] = -ball1_vx[i];
    }
    if ( fix2int15(ball1_y[i]) < 0 )
    {
      ball1_vy[i] = -ball1_vy[i];
    }

    // Ball reborn
    if ( fix2int15(ball1_y[i]) > 470 )
    {
      ball1_x[i] = int2fix15(320);
      ball1_y[i] = int2fix15(0);
      ball1_vx[i] = ((fix15)(rand() & 0xffff) >> 1) - 16384;
      ball1_vy[i] = int2fix15(0);
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
static PT_THREAD (protothread_anim(struct pt *pt))
{
    // Mark beginning of thread
    PT_BEGIN(pt);

    // Variables for maintaining frame rate
    static int begin_time ;
    static int spare_time ;

    // Create a ball
    createBall0();

    // Check screen dimensions
    // fillCircle(   0,   0, 10, WHITE );
    // fillCircle( 640,   0, 10,   RED );
    // fillCircle( 640, 480, 10, GREEN );
    // fillCircle(   0, 480, 10,  BLUE );

    while(1) {
      // Measure time at start of thread
      begin_time = time_us_32();

      // Erase ball
      for (int i = 0; i < ball_num; i++)
      {
        fillCircle(fix2int15(ball0_x[i]), fix2int15(ball0_y[i]), 4, BLACK);
      }

      // Update ball position and velocity
      moveBall0();

      // Draw the ball at new position
      for (int i = 0; i < ball_num; i++)
      {
        fillCircle(fix2int15(ball0_x[i]), fix2int15(ball0_y[i]), 4, ball_color_0);
      }

      // Draw peg
      fillCircle(peg_x_int[0], peg_y_int[0], 6, peg_color);
      for(int i = 0; i < peg_num; i++)
      {
        fillCircle(fix2int15(peg_x[i]), fix2int15(peg_y[i]), 6, peg_color);
      }

      // delay in accordance with frame rate
      spare_time = FRAME_RATE - (time_us_32() - begin_time) ;
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

    // Create a ball
    createBall1();

    while(1) {
      // Measure time at start of thread
      begin_time = time_us_32();

      // Erase ball
      for (int i = 0; i < ball_num; i++)
      {
        fillCircle(fix2int15(ball1_x[i]), fix2int15(ball1_y[i]), 4, BLACK);
      }

      // Update ball position and velocity
      moveBall1();

      // Draw the ball at new position
      for (int i = 0; i < ball_num; i++)
      {
        fillCircle(fix2int15(ball1_x[i]), fix2int15(ball1_y[i]), 4, ball_color_1);
      }

      // Draw peg
      for(int i = 0; i < peg_num; i++)
      {
        fillCircle(fix2int15(peg_x[i]), fix2int15(peg_y[i]), 6, peg_color);
      }
      
      // delay in accordance with frame rate
      spare_time = FRAME_RATE - (time_us_32() - begin_time) ;
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

  // ===========================================
  // ===== CODE FROM DMA DEMO STARTS HERE ==========
  // ===========================================


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

  // ===========================================
  // ===== CODE FROM DMA DEMO ENDS HERE ==========
  // ===========================================

  set_sys_clock_khz(150000, true) ;
  // initialize stio
  // stdio_init_all() ;

  // initialize VGA
  initVGA() ;

  // Create peg
  createPeg();
  
  // Convert parameters
  g = float2fix15(g_float);
  ball_r = int2fix15(ball_r_int);
  peg_r = int2fix15(peg_r_int);
  bounciness = float2fix15(bounciness_float);

  // start core 1 
  multicore_reset_core1();
  multicore_launch_core1(&core1_main);

  // add threads
  pt_add_thread(protothread_serial);
  pt_add_thread(protothread_anim);

  // start scheduler
  pt_schedule_start ;
}