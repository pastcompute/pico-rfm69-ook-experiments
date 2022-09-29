// This program continually samples the RSSI reported by the SX1231
// The sampling is done on 100uS intervals, even though the device calculates it at least 4x faster,
// becuase empirically the SPI and loop overhead means we can't do this any faster than about 70us 
// It prints it to the serial port, integrating over a specified number of bins,
// allowing us to see in realtime a possible nearby signal detection

#include <Arduino.h>
#include <stdio.h>
#include <pico/stdlib.h>
#include "../rfm69common.h"

// Set this to 1 to print every sample binned, otherwise it only prints
// when > 1 point above the long term background
#define PRINT_ALL_VALUES 0


// See ook-demod for a description of these common constants

#include "../picopins.h"

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
    auto nextOutput_us = ONE_SECOND_US /4;
    // integrate and output over one quarter second
    // this scrolls faster but makes the detections more visible given how much shorter than 1 second the transmissions are

    absolute_time_t tNow = get_absolute_time();
    absolute_time_t tNextOutput = delayed_by_us(tNow, nextOutput_us);
    absolute_time_t tNextPoll = delayed_by_us(tNow, rssiPoll_us);
    byte rssi = 0;

    // We expect around 10000 samples per output, with a value E 60, 127 s a float should be sufficient
    int32_t runningSum = 0;
    int runningCount = 0;
    int periods = 0;
    byte floorRssi = 0;

    // Also try and work out the long term mean and subtract that so we can have an axis
    // We'd probably be better off using a geometric or rolling mean but this will do for the time being...
    float longTermMean = +1;
    auto t0 = to_ms_since_boot(get_absolute_time());
    while (true) {
      tNow = get_absolute_time();
      if (time_reached(tNextPoll)) {
        tNextPoll = delayed_by_us(tNow, rssiPoll_us);
        rssi = rfm69.readRSSIByte();
        runningSum += rssi;
        runningCount++;
        if (rssi > floorRssi) { floorRssi = rssi; }
      }
      if (time_reached(tNextOutput)) {
        // Here we are "integrating" the received "energy" above the floor
        // Of course RSSI is dB and relative to "something" but this is a useful proxy still
        // A period with no transmissions will have a lower value...
        // The value has units of dB still
        //float energyProxy = (runningSum - floorRssi * runningCount) / -2.F / runningCount;
        float energyProxy = runningSum / -2.F / runningCount;

        if (longTermMean > 0) {
            longTermMean = energyProxy;
        } else {
            longTermMean = (energyProxy + longTermMean);
        }

        // And this is not using a real time O/S, so perhaps this will delay the next sample slightly
        auto t1 = to_ms_since_boot(get_absolute_time());

        float background = longTermMean / (periods + 1);

        bool detection = energyProxy - background > 1;

        if (periods > 1 && detection) {
            printf("%8.2f %6.1f %6.1f    ", (t1 - t0) / 1000.F, background, energyProxy);

            // Bin this into 5dB slots from -127
            int nx = (energyProxy + 127.5) / 5;
            for (int i=0; i < nx; i++) { printf("*"); } printf("\n");
        } else if (periods < 1) { 
            printf("%8.2f %6.1f (initial integration)\n", (t1 - t0) / 1000.F, background);
        }
        tNextOutput = delayed_by_us(tNow, nextOutput_us);
        runningSum = 0;
        runningCount = 0;
        periods ++;
      }
    }
    return 0;
}
