/**
 * Hunter Adams (vha3@cornell.edu)
 * 
 * Keypad Demo
 * 
 * KEYPAD CONNECTIONS
 *  - GPIO 9   -->  330 ohms  --> Pin 1 (button row 1)
 *  - GPIO 10  -->  330 ohms  --> Pin 2 (button row 2)
 *  - GPIO 11  -->  330 ohms  --> Pin 3 (button row 3)
 *  - GPIO 12  -->  330 ohms  --> Pin 4 (button row 4)
 *  - GPIO 13  -->     Pin 5 (button col 1)
 *  - GPIO 14  -->     Pin 6 (button col 2)
 *  - GPIO 15  -->     Pin 7 (button col 3)
 * 
 * VGA CONNECTIONS
 *  - GPIO 16 ---> VGA Hsync
 *  - GPIO 17 ---> VGA Vsync
 *  - GPIO 18 ---> 470 ohm resistor ---> VGA Green 
 *  - GPIO 19 ---> 330 ohm resistor ---> VGA Green
 *  - GPIO 20 ---> 330 ohm resistor ---> VGA Blue
 *  - GPIO 21 ---> 330 ohm resistor ---> VGA Red
 *  - RP2040 GND ---> VGA GND
 * 
 * SERIAL CONNECTIONS
 *  - GPIO 0        -->     UART RX (white)
 *  - GPIO 1        -->     UART TX (green)
 *  - RP2040 GND    -->     UART GND
 * 
 * KEYPAD FUNCTIONS
 *  - Button 1 (i ==  1) --> Swoop
 *  - Button 2 (i ==  2) --> Chirp
 *  - Button 3 (i ==  3) --> Silence
 *  - Button * (i == 10) --> Record
 *  - Button # (i == 11) --> Play
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/sync.h"
#include "hardware/spi.h"
#include "hardware/clocks.h"

// VGA graphics library
#include "vga16_graphics_v2.h"
#include "pt_cornell_rp2040_v1_4.h"


// Keypad pin configurations
#define BASE_KEYPAD_PIN 9
#define KEYROWS         4
#define NUMKEYS         12

#define LED             25

unsigned int keycodes[12] = {   0x28, 0x11, 0x21, 0x41, 0x12,
                                0x22, 0x42, 0x14, 0x24, 0x44,
                                0x18, 0x48} ;
unsigned int scancodes[4] = {   0x01, 0x02, 0x04, 0x08} ;
unsigned int button = 0x70 ;


char keytext[40];
int prev_key = 0;

// Debouncing FSM
#define NOT_PRESSED        0
#define MAYBE_PRESSED      1
#define PRESSED            2
#define MAYBE_NOT_PRESSED  3
volatile unsigned int DB_STATE = NOT_PRESSED;
volatile unsigned int AC_NEW = 1;

// Rercord FSM
#define FREE    0
#define RECORD  1
#define PLAY    2
volatile unsigned int RC_STATE = FREE;
#define MAX_SONG_LENGTH  100
int key_seq[MAX_SONG_LENGTH] = {0};
int song_index = 0;
int play_index = 0;

// Print counter
volatile unsigned int print_counter = 0;

// ================================================================
// ========================== START BEEP ==========================
// ================================================================

// Low-level alarm infrastructure we'll be using
#define ALARM_NUM 0
#define ALARM_IRQ TIMER_IRQ_0

// Macros for fixed-point arithmetic (faster than floating point)
typedef signed int fix15 ;
#define multfix15(a,b) ((fix15)((((signed long long)(a))*((signed long long)(b)))>>15))
#define float2fix15(a) ((fix15)((a)*32768.0)) 
#define fix2float15(a) ((float)(a)/32768.0)
#define absfix15(a) abs(a) 
#define int2fix15(a) ((fix15)(a << 15))
#define fix2int15(a) ((int)(a >> 15))
#define char2fix15(a) (fix15)(((fix15)(a)) << 15)
#define divfix(a,b) (fix15)( (((signed long long)(a)) << 15) / (b))

//Direct Digital Synthesis (DDS) parameters
#define two32 4294967296.0  // 2^32 (a constant)
#define Fs 50000
#define DELAY 20 // 1/Fs (in microseconds)

// the DDS units - core 0
// Phase accumulator and phase increment. Increment sets output frequency.
volatile unsigned int phase_accum_main_0;
//volatile unsigned int phase_incr_main_0 = (400.0*two32)/Fs ;
volatile unsigned int phase_incr_main_0;

// Frequency modulation
volatile unsigned int freq;
unsigned int two32Fs = two32/Fs;

// DDS sine table (populated in main())
#define sine_table_size 256
fix15 sin_table[sine_table_size] ;
#define freq_table_size 407
fix15 swoop_table[freq_table_size] ;
fix15 chirp_table[freq_table_size] ;

// Values output to DAC
int DAC_output_0 ;
int DAC_output_1 ;

// Amplitude modulation parameters and variables
fix15 max_amplitude = int2fix15(1) ;    // maximum amplitude
fix15 attack_inc ;                      // rate at which sound ramps up
fix15 decay_inc ;                       // rate at which sound ramps down
fix15 current_amplitude_0 = 0 ;         // current amplitude (modified in ISR)
fix15 current_amplitude_1 = 0 ;         // current amplitude (modified in ISR)

// Timing parameters for beeps (units of interrupts)
#define ATTACK_TIME             250
#define DECAY_TIME              250
#define SUSTAIN_TIME            6000
#define BEEP_DURATION           6500
#define BEEP_REPEAT_INTERVAL    20000

// State machine variables
#define IDLE     0
#define SWOOP    1
#define CHIRP    2
#define SILENCE  3
volatile unsigned int BP_STATE = IDLE ;
volatile unsigned int counter = 0 ;

// SPI data
uint16_t DAC_data_1 ; // output value
uint16_t DAC_data_0 ; // output value

// DAC parameters (see the DAC datasheet)
// A-channel, 1x, active
#define DAC_config_chan_A 0b0011000000000000
// B-channel, 1x, active
#define DAC_config_chan_B 0b1011000000000000

//SPI configurations (note these represent GPIO number, NOT pin number)
#define PIN_MISO 4
#define PIN_CS   5
#define PIN_SCK  6
#define PIN_MOSI 7
#define LDAC     8
#define LED      25
#define SPI_PORT spi0

//GPIO for timing the ISR
#define ISR_GPIO 2

// This timer ISR is called on core 0
static void alarm_irq(void) {

    // Assert a GPIO when we enter the interrupt
    gpio_put(ISR_GPIO, 1) ;

    // Clear the alarm irq
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);

    // Reset the alarm register
    timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + DELAY ;

    if ( BP_STATE == SWOOP ) {
        // Frequency modulation table lookup
        freq = fix2int15(swoop_table[counter>>4]);
        phase_incr_main_0 = freq * two32Fs;
        // DDS phase and sine table lookup
        phase_accum_main_0 += phase_incr_main_0  ;
        DAC_output_0 = fix2int15(multfix15(current_amplitude_0,
            sin_table[phase_accum_main_0>>24])) + 2048 ;

        // Ramp up amplitude
        if (counter < ATTACK_TIME) {
            current_amplitude_0 = (current_amplitude_0 + attack_inc) ;
        }
        // Ramp down amplitude
        else if (counter > BEEP_DURATION - DECAY_TIME) {
            current_amplitude_0 = (current_amplitude_0 - decay_inc) ;
        }

        // Mask with DAC control bits
        DAC_data_0 = (DAC_config_chan_B | (DAC_output_0 & 0xffff))  ;

        // SPI write (no spinlock b/c of SPI buffer)
        spi_write16_blocking(SPI_PORT, &DAC_data_0, 1) ;

        // Increment the counter
        counter += 1 ;

        // State transition?
        if (counter == BEEP_DURATION) {
            BP_STATE = IDLE ;
            counter = 0 ;
        }
    } else if ( BP_STATE == CHIRP ) {
        // Frequency modulation table lookup
        freq = fix2int15(chirp_table[counter>>4]);
        phase_incr_main_0 = freq * two32Fs;
        // DDS phase and sine table lookup
        phase_accum_main_0 += phase_incr_main_0  ;
        DAC_output_0 = fix2int15(multfix15(current_amplitude_0,
            sin_table[phase_accum_main_0>>24])) + 2048 ;

        // Ramp up amplitude
        if (counter < ATTACK_TIME) {
            current_amplitude_0 = (current_amplitude_0 + attack_inc) ;
        }
        // Ramp down amplitude
        else if (counter > BEEP_DURATION - DECAY_TIME) {
            current_amplitude_0 = (current_amplitude_0 - decay_inc) ;
        }

        // Mask with DAC control bits
        DAC_data_0 = (DAC_config_chan_B | (DAC_output_0 & 0xffff))  ;

        // SPI write (no spinlock b/c of SPI buffer)
        spi_write16_blocking(SPI_PORT, &DAC_data_0, 1) ;

        // Increment the counter
        counter += 1 ;

        // State transition?
        if (counter == BEEP_DURATION) {
            BP_STATE = IDLE ;
            counter = 0 ;
        }
    } else if ( BP_STATE == SILENCE ) {
        counter += 1;
        if ( counter == BEEP_DURATION ) {
            BP_STATE = IDLE;
            counter = 0;
        }
    }

    // State transition?
    else {
        // counter += 1 ;
        // if (counter == BEEP_REPEAT_INTERVAL) {
        //     current_amplitude_0 = 0 ;
        //     BP_STATE = SWOOP ;
        //     counter = 0 ;
        // }
        current_amplitude_0 = 0;
    }

    // De-assert the GPIO when we leave the interrupt
    gpio_put(ISR_GPIO, 0) ;

}

// ================================================================
// =========================== END BEEP ===========================
// ================================================================

// This thread runs on core 0
static PT_THREAD (protothread_core_0(struct pt *pt))
{
    // Indicate thread beginning
    PT_BEGIN(pt) ;

    // Some variables
    static int i ;
    static int i_possible;
    static uint32_t keypad ;

    while(1) {

        gpio_put(LED, !gpio_get(LED)) ;

        // Scan the keypad!
        for (i=0; i<KEYROWS; i++) {
            // Set a row high
            gpio_put_masked((0xF << BASE_KEYPAD_PIN),
                            (scancodes[i] << BASE_KEYPAD_PIN)) ;
            // Small delay required
            sleep_us(1) ; 
            // Read the keycode
            keypad = ((gpio_get_all() >> BASE_KEYPAD_PIN) & 0x7F) ;
            // Break if button(s) are pressed
            if (keypad & button) break ;
        }
        // If we found a button . . .
        if (keypad & button) {
            // Look for a valid keycode.
            for (i=0; i<NUMKEYS; i++) {
                if (keypad == keycodes[i]) break ;
            }
            // If we don't find one, report invalid keycode
            if (i==NUMKEYS) (i = -1) ;
        }
        // Otherwise, indicate invalid/non-pressed buttons
        else (i=-1) ;



        // Write key to VGA
        if (i != prev_key) {
            prev_key = i ;
            fillRect(250, 20, 176, 30, RED); // red box
            sprintf(keytext, "%d", i) ;
            setCursor(250, 20) ;
            setTextSize(2) ;
            writeString(keytext) ;
        }

        // Debouncing FSM
        if ( DB_STATE == NOT_PRESSED ) {
            if ( i == -1 ) {
                DB_STATE = NOT_PRESSED;
            } else {
                i_possible = i;
                DB_STATE = MAYBE_PRESSED;
            }
        } else if ( DB_STATE == MAYBE_PRESSED ) {
            if ( i == i_possible ) {
                DB_STATE = PRESSED;
            } else {
                DB_STATE = NOT_PRESSED;
            }
        } else if ( DB_STATE == PRESSED ) {
            if ( i == i_possible ) {
                DB_STATE = PRESSED;
            } else {
                DB_STATE = MAYBE_NOT_PRESSED;
            }
        } else if ( DB_STATE == MAYBE_NOT_PRESSED ) {
            if ( i == i_possible ) {
                DB_STATE = PRESSED;
            } else {
                DB_STATE = NOT_PRESSED;
            }
        }

        if ( DB_STATE == PRESSED && AC_NEW == 1 && BP_STATE == IDLE ) {
            if ( i == 1 ) {
                BP_STATE = SWOOP;
                AC_NEW = 0;
                if ( RC_STATE == RECORD ) {
                    key_seq[song_index] = i;
                    song_index += 1;
                }
            } else if ( i == 2 ) {
                BP_STATE = CHIRP;
                AC_NEW = 0;
                if ( RC_STATE == RECORD ) {
                    key_seq[song_index] = i;
                    song_index += 1;
                }
            } else if ( i == 3 ) {
                BP_STATE = SILENCE;
                AC_NEW = 0;
                if ( RC_STATE == RECORD ) {
                    key_seq[song_index] = i;
                    song_index += 1;
                }
            } else if (i == 10) {
                RC_STATE = RECORD;
                AC_NEW = 0;
                for (int i = 0; i < song_index; i++) {
                    key_seq[i] = 0;
                }
                song_index = 0;
            } else if (i == 11) {
                RC_STATE = PLAY;
                AC_NEW = 0;
            } else {
                BP_STATE = IDLE;
            }
        }

        if ( DB_STATE == NOT_PRESSED ) {
            AC_NEW = 1;
        }

        // Record FSM
        if ( RC_STATE == PLAY ) {
            if ( key_seq[play_index] == 0 ) {
                RC_STATE = FREE;
                play_index = 0;
            } else {
                if ( BP_STATE == IDLE ) {
                    BP_STATE = key_seq[play_index];
                    play_index += 1;
                }
            }
        } else if ( RC_STATE == RECORD ) {
        } else if ( RC_STATE == FREE ) {
        }

        // Print key to terminal
        if ( print_counter == 10 ) {
            printf("\n Keyscan %d  DB_STATE %d  BP_STATE %d  RC_STATE %d  play_index %d", 
                i, DB_STATE, BP_STATE, RC_STATE, play_index) ;
            printf("\n key_seq %d %d %d %d %d", 
                key_seq[0], key_seq[1], key_seq[2], key_seq[3], key_seq[4] );
            print_counter = 0;
        }
        print_counter += 1;

        PT_YIELD_usec(30000) ;
    }
    // Indicate thread end
    PT_END(pt) ;
}


int main() {

    // Overclock
    set_sys_clock_khz(150000, true) ;

    // Initialize stdio
    stdio_init_all();

    // Initialize the VGA screen
    initVGA() ;

    // Draw some filled rectangles
    fillRect(64, 0, 176, 50, BLUE); // blue box
    fillRect(250, 0, 176, 50, RED); // red box
    fillRect(435, 0, 176, 50, GREEN); // green box

    // Write some text
    setTextColor(WHITE) ;
    setCursor(65, 0) ;
    setTextSize(1) ;
    writeString("Raspberry Pi Pico") ;
    setCursor(65, 10) ;
    writeString("Keypad demo") ;
    setCursor(65, 20) ;
    writeString("Hunter Adams") ;
    setCursor(65, 30) ;
    writeString("vha3@cornell.edu") ;
    setCursor(250, 0) ;
    setTextSize(2) ;
    writeString("Key Pressed:") ;

    // Map LED to GPIO port, make it low
    gpio_init(LED) ;
    gpio_set_dir(LED, GPIO_OUT) ;
    gpio_put(LED, 0) ;

    ////////////////// KEYPAD INITS ///////////////////////
    // Initialize the keypad GPIO's
    gpio_init_mask((0x7F << BASE_KEYPAD_PIN)) ;
    // Set row-pins to output
    gpio_set_dir_out_masked((0xF << BASE_KEYPAD_PIN)) ;
    // Set all output pins to low
    gpio_put_masked((0xF << BASE_KEYPAD_PIN), (0x0 << BASE_KEYPAD_PIN)) ;
    // Turn on pulldown resistors for column pins (on by default)
    gpio_pull_down((BASE_KEYPAD_PIN + 4)) ;
    gpio_pull_down((BASE_KEYPAD_PIN + 5)) ;
    gpio_pull_down((BASE_KEYPAD_PIN + 6)) ;

    // ================================================================
    // ========================== START BEEP ==========================
    // ================================================================

    // Initialize stdio/uart (printf won't work unless you do this!)
    //stdio_init_all();
    printf("Hello, friends!\n");

    // Initialize SPI channel (channel, baud rate set to 20MHz)
    spi_init(SPI_PORT, 20000000) ;
    // Format (channel, data bits per transfer, polarity, phase, order)
    spi_set_format(SPI_PORT, 16, 0, 0, 0);

    // Map SPI signals to GPIO ports
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS, GPIO_FUNC_SPI) ;

    // Map LDAC pin to GPIO port, hold it low (could alternatively tie to GND)
    gpio_init(LDAC) ;
    gpio_set_dir(LDAC, GPIO_OUT) ;
    gpio_put(LDAC, 0) ;

    // Setup the ISR-timing GPIO
    gpio_init(ISR_GPIO) ;
    gpio_set_dir(ISR_GPIO, GPIO_OUT);
    gpio_put(ISR_GPIO, 0) ;

    // Map LED to GPIO port, make it low
    //gpio_init(LED) ;
    //gpio_set_dir(LED, GPIO_OUT) ;
    //gpio_put(LED, 0) ;

    // set up increments for calculating bow envelope
    attack_inc = divfix(max_amplitude, int2fix15(ATTACK_TIME)) ;
    decay_inc =  divfix(max_amplitude, int2fix15(DECAY_TIME)) ;

    // Build the sine lookup table
    // scaled to produce values between 0 and 4096 (for 12-bit DAC)
    int ii;
    for (ii = 0; ii < sine_table_size; ii++){
         sin_table[ii] = float2fix15(2047*sin((float)ii*6.283/(float)sine_table_size));
    }

    // Build frequency modulation lookup table
    for (int x = 0; x < freq_table_size; x++) {
        swoop_table[x] = float2fix15( 260 * sin((float)3.1415*x/(float)406.25) + 1740 );
    }
    for (int x = 0; x < freq_table_size; x++) {
        chirp_table[x] = float2fix15( 0.03015*x*x + 2000 );
    }

    // Enable the interrupt for the alarm (we're using Alarm 0)
    hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM) ;
    // Associate an interrupt handler with the ALARM_IRQ
    irq_set_exclusive_handler(ALARM_IRQ, alarm_irq) ;
    // Enable the alarm interrupt
    irq_set_enabled(ALARM_IRQ, true) ;
    // Write the lower 32 bits of the target time to the alarm register, arming it.
    timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + DELAY ;

    // Add core 0 threads
    //pt_add_thread(protothread_core_0) ;

    // Start scheduling core 0 threads
    //pt_schedule_start ;

    // ================================================================
    // =========================== END BEEP ===========================
    // ================================================================

    // Add core 0 threads
    pt_add_thread(protothread_core_0) ;

    // Start scheduling core 0 threads
    pt_schedule_start ;

}

// Last edit: 2025.09.12 01:01