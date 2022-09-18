// This program tunes the receiver and enables continuous OOK demodulation with output on DIO2
// It is suitable for triggering a logic analyser and experimenting with settings
//
// It was inspired by https://FIXME
//
// When using sigrok, use a command like the following:
//
// sigrok-cli -d fx2lafw --config samplerate=1000000 --channels D5=ASK,D1=TRG  --samples 400000  --triggers TRG=01 -o capture.sr
//
// In this example:
//  Logic Analyser D5 --> RFM69 DIO2
//  Logic Analyser D1 --> Pico GP16
// /When this program samples the SX1231 RSSI register and it is above the threshold
// (in this case -90dB) it will trigger the logic analyser to capture 400ms
// if successful, you can load the output file, e.g. capture.sr, into Pulseview and
// use the OOK and Oregon decoders if recieving Oregon temperature sensor transmissions
// Run sigrok-cli after the program has started, to ensure the trigger wont glitch on start.

#include <Arduino.h>
#include <stdio.h>
#include <pico/stdlib.h>

#include "../rfm69common.h"

// With the arduino-compat shim, Arduino pins Dn is exactly the same as Pico SDK pin GPn

// I adopt one pin to intentionally trigger the logic analyser when the program starts,
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

#define RF_FREQUENCY_MHZ 433.92

#define ONE_SECOND_US (1000 * 1000)

// This is the RSSI to use to trigger the logic analyser
#define ESTIMATED_TRIGGER_RSSI_DB -90

int main() {
    // Trigger for the logic analyser, corresponds to the first pulse in RSSI
    pinMode(LOGIC_TRIGGER, OUTPUT);
    digitalWrite(LOGIC_TRIGGER, LOW);

    stdio_init_all();

    Rfm69Common rfm69;
    rfm69.setPins(RFM69_MISO, RFM69_MOSI, RFM69_SCK, RFM69_CS, RFM69_IRQ, RFM69_RST);
    rfm69.begin(RF_FREQUENCY_MHZ);

    // To help with triggering our logic analyser, we can now poll
    // what the module thinks the RSSI is, and use our own threshold.
    // This will only work because we can control where we put the transmitter relative
    // to our device; in the real world, RSSI is generally thresholded using
    // some kind of AGC, because we wont otherwise know what the level should be!
    const int rssiPoll_us = 100;
    const float triggerRSSI_db = ESTIMATED_TRIGGER_RSSI_DB;
    const uint8_t triggerByte = -(2.0 * triggerRSSI_db);

    // we know the transmission is usually about 180ms long and there are two in a row
    // so after triggering, reset after 400ms
    // compute how long this should be in polling interals
    // if the logic analyser high period differs from the actual time
    // this is how we detect if rssiPoll_us is significantly too short
    // for accuracy, dont printf in the middle of it
    const int messageCaptureSamples = ONE_SECOND_US / rssiPoll_us * 2 / 5;

    absolute_time_t tNow = get_absolute_time();
    absolute_time_t tNextSecond = delayed_by_us(tNow, ONE_SECOND_US);
    absolute_time_t tNextPoll = delayed_by_us(tNow, rssiPoll_us);
    int n=0;
    byte rssi = 0;
    bool triggered = false;
    int triggeringSamples = 0;
    float triggeredAtRssi = 0;
    while (true) {
      tNow = get_absolute_time();

      // sample RSSI, see if we have a real signal for our lab setup
      if (time_reached(tNextPoll)) {
        tNextPoll = delayed_by_us(tNow, rssiPoll_us);
        rssi = rfm69.readRSSIByte();

        if (!triggered && rssi <= triggerByte) {
            // trigger the Logic Analyser
            digitalWrite(LOGIC_TRIGGER, HIGH);
            triggered = true;
            triggeredAtRssi = rssi;
        }
        if (triggered) {
            triggeringSamples++;
            if (triggeringSamples == messageCaptureSamples) {
                digitalWrite(LOGIC_TRIGGER, LOW);
                triggered = false;
                triggeringSamples = 0;
                printf("\nTriggered at %.1fdB after %d seconds\n", triggeredAtRssi / -2.F, n);            
            }
        }
      }

      // Print something so we know we are not hung
      if (!triggered && time_reached(tNextSecond)) {
          tNextSecond = delayed_by_us(tNow, ONE_SECOND_US);
          printf((n % 2 == 0) ? "-\r" : "|\r");
          n++;
      }
      if (!triggered) {
        // Loop needs to use sleep_ms or the Serial port wont function
        sleep_ms(1);
      }
    }
    return 0;
}
