#ifndef APPS_PINS_H_
#define APPS_PINS_H_

// Allow these to be easily changed for all examples

// In my testing, I happened to use these Pico Pins, and software SPI
// I havent tried adapting RadioHead to use the pico hardware SPI as yet
// Connect these to the RFM69HCW module

#define LOGIC_TRIGGER D16

#define RFM69_MISO D4
#define RFM69_MOSI D7
#define RFM69_SCK D6
#define RFM69_CS D5

#define RFM69_RST D12
#define RFM69_IRQ D10

#define RFM69_DIO2 D11

#endif
