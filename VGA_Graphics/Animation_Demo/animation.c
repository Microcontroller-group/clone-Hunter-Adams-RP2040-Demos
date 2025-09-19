
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
// Include protothreads
#include "pt_cornell_rp2040_v1_4.h"

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

// the color of the boid
char color = WHITE ;

// Gravity parameter
float g_float = 5.0;
fix15 g;

// Ball and peg parameters
int ball_r_int =   4;
int peg_r_int =    6;
int peg_x_int =  320;
int peg_y_int =  240;
float bounciness_float = 0.5;
fix15 ball_r;
fix15 peg_r;
fix15 peg_x;
fix15 peg_y;
fix15 bounciness;

// Ball on core 0
fix15 ball0_x;
fix15 ball0_y;
fix15 ball0_vx;
fix15 ball0_vy;

// Ball on core 1
fix15 ball1_x;
fix15 ball1_y;
fix15 ball1_vx;
fix15 ball1_vy;

// Create ball
void createBall(fix15* x, fix15* y, fix15* vx, fix15* vy)
{
  // Start in center top
  *x = int2fix15(320);
  *y = int2fix15(0);
  *vx = ((fix15)(rand() & 0xffff) >> 1) - 16384;
  *vy = int2fix15(0);
}

// Update ball position and velocity
void moveBall(fix15* x, fix15* y, fix15* vx, fix15* vy, fix15 g)
{
  // Peg collision
  fix15 dx = *x - peg_x;
  fix15 dy = *y - peg_y;
  if ( (abs(dx) < ball_r + peg_r) && (abs(dy) < ball_r + peg_r) )
  {
    fix15 distance = sqrtfix(multfix15(dx, dx) + multfix15(dy, dy));

    fix15 normal_x = divfix(dx, distance);
    fix15 normal_y = divfix(dy, distance);

    fix15 intermediate_term = multfix15(int2fix15(-2), (multfix15(normal_x,  *vx) + multfix15(normal_y, *vy)));

    if ( intermediate_term > int2fix15(0))
    {
      //ball.x = peg.x + (normal_x * (distance+1))
      *x = peg_x + multfix15(normal_x, (distance + int2fix15(1)));
      *y = peg_y + multfix15(normal_y, (distance + int2fix15(1)));
      //ball.vx = ball.vx + (normal_x * intermediate_term)
      *vx = *vx + multfix15(normal_x, intermediate_term);
      *vy = *vy + multfix15(normal_y, intermediate_term);
      *vx = multfix15(*vx, bounciness);
      *vy = multfix15(*vy, bounciness);
    }
  }

  // Hit walls
  if ( (fix2int15(*x) < 0) || (fix2int15(*x) > 640) )
  {
    *vx = -*vx;
  }
  if ( fix2int15(*y) < 0 )
  {
    *vy = -*vy;
  }

  // Ball reborn
  if ( fix2int15(*y) > 470 )
  {
    *x = int2fix15(320);
    *y = int2fix15(0);
    *vx = ((fix15)(rand() & 0xffff) >> 1) - 16384;
    *vy = int2fix15(0);
  }

  // Gravity
  *vy = *vy + g;

  // Update position
  *x = *x + *vx;
  *y = *y + *vy;
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
          color = (char)user_input ;
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
    createBall(&ball0_x, &ball0_y, &ball0_vx, &ball0_vy);

    // Check screen dimensions
    // fillCircle(   0,   0, 10, WHITE );
    // fillCircle( 640,   0, 10,   RED );
    // fillCircle( 640, 480, 10, GREEN );
    // fillCircle(   0, 480, 10,  BLUE );

    while(1) {
      // Measure time at start of thread
      begin_time = time_us_32() ;      
      // Erase ball
      fillCircle(fix2int15(ball0_x), fix2int15(ball0_y), 4, BLACK);
      // Update ball position and velocity
      moveBall(&ball0_x, &ball0_y, &ball0_vx, &ball0_vy, g); 
      // Draw the ball at new position
      fillCircle(fix2int15(ball0_x), fix2int15(ball0_y), 4, BLUE);
      // Draw peg
      fillCircle(320, 240, 6, GREEN);
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
    createBall(&ball1_x, &ball1_y, &ball1_vx, &ball1_vy);

    while(1) {
      // Measure time at start of thread
      begin_time = time_us_32() ;      
      // Erase ball
      fillCircle(fix2int15(ball1_x), fix2int15(ball1_y), 4, BLACK);
      // Update ball position and velocity
      moveBall(&ball1_x, &ball1_y, &ball1_vx, &ball1_vy, g); 
      // Draw the ball at new position
      fillCircle(fix2int15(ball1_x), fix2int15(ball1_y), 4, RED);
      // Draw peg
      fillCircle(320, 240, 6, GREEN);
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
  set_sys_clock_khz(150000, true) ;
  // initialize stio
  stdio_init_all() ;

  // initialize VGA
  initVGA() ;
  
  // Convert parameters
  g = float2fix15(g_float/10);
  ball_r = int2fix15(ball_r_int);
  peg_r = int2fix15(peg_r_int);
  peg_x = int2fix15(peg_x_int);
  peg_y = int2fix15(peg_y_int);
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
