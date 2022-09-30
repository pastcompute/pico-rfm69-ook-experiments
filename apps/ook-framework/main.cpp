// Simple frameork for copying to start additional experiments

#include <Arduino.h>
#include <stdio.h>
#include <pico/stdlib.h>
#include "../rfm69common.h"
#include "DecodeOOK.h"
#include "OregonDecoderV2.h"

#include "../picopins.h"

// See ook-demod for a description of these common constants

#define RF_FREQUENCY_MHZ 433.92

#define ONE_SECOND_US (1000 * 1000)
#define OREGON_CHIPRATE (1024  * 2)

#define ESTIMATED_TRIGGER_RSSI_DB -90


int main() {
    pinMode(LOGIC_TRIGGER, OUTPUT);
    digitalWrite(LOGIC_TRIGGER, LOW);

    stdio_init_all();

    pinMode(RFM69_DIO2, INPUT_PULLDOWN);

    Rfm69Common rfm69;
    rfm69.setPins(RFM69_MISO, RFM69_MOSI, RFM69_SCK, RFM69_CS, RFM69_IRQ, RFM69_RST);
    rfm69.begin(RF_FREQUENCY_MHZ);

    printf("Start...\n");
    absolute_time_t tNow = get_absolute_time();
    absolute_time_t tNextSecond = delayed_by_us(tNow, ONE_SECOND_US);
    int n=0;
    float rssi;
    auto t0 = to_ms_since_boot(tNow);
    while (true) {
        if (time_reached(tNextSecond)) {
            auto t1 = to_ms_since_boot(get_absolute_time());
            tNow = get_absolute_time();
            tNextSecond = delayed_by_us(tNow, ONE_SECOND_US);
            rssi = rfm69.readRSSIByte() / -2.0F;
            printf((n % 2 == 0) ? "- %d %.1f    \r" : "| %d %.1f    \r", (t1 - t0)/1000, rssi);
            sleep_ms(1);
            n++;
        }
    }
    return 0;
}
