/**
 * DAC (Digital-to-Analog Converter) interface module
 * Handles SPI communication with the DAC
 */

#ifndef DAC_H
#define DAC_H

#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

// DAC parameters (see the DAC datasheet)
#define DAC_config_chan_A 0b0011000000000000  // A-channel, 1x, active
#define DAC_config_chan_B 0b1011000000000000  // B-channel, 1x, active

// SPI configurations (note these represent GPIO number, NOT pin number)
#define PIN_MISO 4
#define PIN_CS   5
#define PIN_SCK  6
#define PIN_MOSI 7
#define LDAC     8
#define SPI_PORT spi0

// Function prototypes
void dac_init(void);
void dac_write_channel_b(int16_t value);

#endif // DAC_H
