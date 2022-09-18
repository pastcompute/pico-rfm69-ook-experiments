// This program reports the timing information between OOK pulses using an IRQ
// This is the first step toward decoding OOK signals
// For now we are still using RSSI triggering to kick things off

#include <Arduino.h>
#include <stdio.h>
#include <pico/stdlib.h>
#include "../rfm69common.h"

// See ook-demod for a description of these common constants

#define LOGIC_TRIGGER D16

#define RFM69_MISO D12
#define RFM69_MOSI D15
#define RFM69_SCK D14
#define RFM69_CS D13

#define RFM69_RST D21
#define RFM69_IRQ D19

#define RFM69_DIO2 D18

#define RF_FREQUENCY_MHZ 433.92

#define ONE_SECOND_US (1000 * 1000)
#define OREGON_CHIPRATE (1024  * 2)

#define ESTIMATED_TRIGGER_RSSI_DB -90

struct shared_data_t {
    volatile uint32_t edgesCount;
    volatile uint32_t nextPulseLength_us;
    volatile uint32_t now;
};

static shared_data_t sharedData = {
    .edgesCount = 0,
    .nextPulseLength_us = 0,
    .now = 0,
};

int main() {
    pinMode(LOGIC_TRIGGER, OUTPUT);
    digitalWrite(LOGIC_TRIGGER, LOW);

    stdio_init_all();

    pinMode(RFM69_DIO2, INPUT_PULLDOWN);

    Rfm69Common rfm69;
    rfm69.setPins(RFM69_MISO, RFM69_MOSI, RFM69_SCK, RFM69_CS, RFM69_IRQ, RFM69_RST);
    rfm69.begin(RF_FREQUENCY_MHZ);

    const int rssiPoll_us = 100;
    const float triggerRSSI_db = ESTIMATED_TRIGGER_RSSI_DB;
    const uint8_t triggerByte = -(2.0 * triggerRSSI_db);

    const int messageCaptureSamples = ONE_SECOND_US / rssiPoll_us * 2 / 5;

    absolute_time_t tNow = get_absolute_time();
    absolute_time_t tNextSecond = delayed_by_us(tNow, ONE_SECOND_US);
    absolute_time_t tNextPoll = delayed_by_us(tNow, rssiPoll_us);
    int n=0;
    byte rssi = 0;
    bool triggered = false;
    int triggeringSamples = 0;
    float triggeredAtRssi = 0;

    int shortPulses = 0, longPulses = 0;

    // Setup interrpts on DIO2
    // Because we are expecting manchester encoding, we want to trigger both rising and falling edges
    extern void dio2InterruptHandler();
    sharedData.edgesCount = 0;
    sharedData.nextPulseLength_us = 0;
    attachInterrupt(digitalPinToInterrupt(RFM69_DIO2), dio2InterruptHandler, CHANGE);

    while (true) {
      if (triggered) {
        noInterrupts();
        auto pulseLength_us = sharedData.nextPulseLength_us;
        sharedData.nextPulseLength_us = 0;
        auto now_us = sharedData.now;
        interrupts();
        // For our case, valid pulses are either ~500uS or ~1msec wide, whether 1 or 0
        // This is a function of the 1024bps rate
        // Use this to try and more accurately count time in chirps, or at least, mask noise
        auto since_us = micros() - now_us;
        bool hadStopped = since_us > OREGON_CHIPRATE * 3;
        if (pulseLength_us > 0 || hadStopped) { // also detect extended no signal
          // these need to be wide enough to deal with intermittent latency
          bool maybeShort = pulseLength_us > 390 && pulseLength_us < 600;
          bool maybeLong = pulseLength_us > 850 && pulseLength_us < 1200;
          // shortest seen in the logic analyser was 880 or 405; if there was a delay servicing th start that could get exaggerated
          // If neither is a valid pulse, lower TRG so it shows on the PulseView output
          // gaps come after the interval that caused it...
          if (maybeShort || maybeLong) {
              digitalWrite(LOGIC_TRIGGER, HIGH);
          } else {
              digitalWrite(LOGIC_TRIGGER, LOW);
          }
          if (maybeShort) {
              shortPulses++;
          }
          if (maybeLong) {
              longPulses++;
          }
          // Attempt to decode manchester here
        }
      }
      tNow = get_absolute_time();

      // sample RSSI, see if we have a real signal for our lab setup
      if (time_reached(tNextPoll)) {
        tNextPoll = delayed_by_us(tNow, rssiPoll_us);
        // we can optimise this, once we trigger, in this application anyway, we dont care about rssi anymore
        if (!triggered) {
            rssi = rfm69.readRSSIByte();
        }
        if (!triggered && rssi <= triggerByte) {
            // trigger the Logic Analyser
            digitalWrite(LOGIC_TRIGGER, HIGH);
            noInterrupts();
            sharedData.edgesCount = 0;
            sharedData.nextPulseLength_us = 0;
            interrupts();
            triggered = true;
            triggeredAtRssi = rssi;
        }
        if (triggered) {
            triggeringSamples++;
            // TOO: use a time reached instead
            if (triggeringSamples == messageCaptureSamples) {
                noInterrupts();
                int edgesCount = sharedData.edgesCount;
                sharedData.edgesCount = 0;
                sharedData.nextPulseLength_us = 0;
                interrupts();

                digitalWrite(LOGIC_TRIGGER, LOW);
                triggered = false;
                triggeringSamples = 0;
                printf("\nTriggered at %.1fdB after %d seconds.\n", triggeredAtRssi / -2.F, n);            
                printf("Number of edges: %d short pulses: %d long pulses: %d\n", edgesCount, shortPulses, longPulses);
                shortPulses = 0;
                longPulses = 0;
            }
        }
      }
      if (!triggered && time_reached(tNextSecond)) {
          tNextSecond = delayed_by_us(tNow, ONE_SECOND_US);
          printf((n % 2 == 0) ? "-\r" : "|\r");
          n++;
      }
      if (!triggered) {
        sleep_ms(1);
      }
    }
    return 0;
}

void dio2InterruptHandler() {
    static uint32_t prevTime_us = 0;

    // From the second and successive interrupt, sharedData.nextPulseLength_us will hold the time between successive interrupts
    // and local prevTime_us will track what micros() was
    // sharedData.nextPulseLength_us is cleared after being processed in the loop
    auto now = micros();
    sharedData.now = now;
    sharedData.nextPulseLength_us = now - prevTime_us;
    prevTime_us = now;
    sharedData.edgesCount++;
}
