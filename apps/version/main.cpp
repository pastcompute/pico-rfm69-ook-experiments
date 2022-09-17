// This program simply reads the version of the SX1231H chip and prints it to the serial port
// Because we need RadioHead we relax and use delay and digitalWrite, etc. from arduino-compat

#include <Arduino.h>
#include <stdio.h>
#include <pico/stdlib.h>
#include <RH_RF69.h>
#include <RHSoftwareSPI.h>

// With the arduino-compat shim, Arduino pins Dn is exactly the same as Pico SDK pin GPn

// I adopt one pin to intentiopnally trigger the logic analyser when the program starts,
// this can be useful at times

#define LOGIC_TRIGGER D16

// In my testing, I happened to use these Pico Pins, and software SPI
// I havent tried adapting RadioHead to use the pico hardware SPI as yet
// Connect these to the RFM69HCW module
#define RFM69_MISO D12
#define RFM69_MOSI D15
#define RFM69_SCK D14
#define RFM69_CS D13
#define RFM69_RST D21
#define RFM69_IRQ D19

int main() {
    // Trigger for the logic analyser, corresponds to the first pulse in RSSI
    pinMode(LOGIC_TRIGGER, OUTPUT);
    // Adjust or move these lines as needed
    // digitalWrite(LOGIC_TRIGGER, LOW);
    digitalWrite(LOGIC_TRIGGER, HIGH);

    stdio_init_all();

    RHSoftwareSPI spi;
    spi.setPins(RFM69_MISO, RFM69_MOSI, RFM69_SCK);

    RH_RF69 rf69module(RFM69_CS, RFM69_IRQ, spi);

    // Reset the module first.
    // From the SX1231 data sheet, pulse RST for 100 uS then wait at least 5 ms
    // We go a bit longer to make sure
    printf("SX1231 reset...\n");
    pinMode(RFM69_RST, OUTPUT);
    digitalWrite(RFM69_RST, HIGH); delay(10);
    digitalWrite(RFM69_RST, LOW); delay(10);
    if (!rf69module.init()) {
        panic("Failed to initialise the RFM69 - probably this is a SPI problem");
    }

    byte versionCode = rf69module.spiRead(RH_RF69_REG_10_VERSION);
    printf("SX1231 version code %02x\n", versionCode);

    return 0;
}
