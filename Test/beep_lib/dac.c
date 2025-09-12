/**
 * DAC (Digital-to-Analog Converter) implementation
 */

#include <stdint.h>
#include "dac.h"

void dac_init(void) {
    // Initialize SPI channel (channel, baud rate set to 20MHz)
    spi_init(SPI_PORT, 20000000);
    // Format (channel, data bits per transfer, polarity, phase, order)
    spi_set_format(SPI_PORT, 16, 0, 0, 0);

    // Map SPI signals to GPIO ports
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS, GPIO_FUNC_SPI);

    // Map LDAC pin to GPIO port, hold it low (could alternatively tie to GND)
    gpio_init(LDAC);
    gpio_set_dir(LDAC, GPIO_OUT);
    gpio_put(LDAC, 0);
}

void dac_write_channel_b(int16_t value) {
    uint16_t dac_data = (DAC_config_chan_B | (value & 0xffff));
    spi_write16_blocking(SPI_PORT, &dac_data, 1);
}
