// Try and use PIO to capture OOK

#include <Arduino.h>
#include <stdio.h>
#include <pico/stdlib.h>
#include "../rfm69common.h"
#include "DecodeOOK.h"
#include "OregonDecoderV2.h"

#include <hardware/pio.h>

#include "../picopins.h"

#include "sample.pio.h"

// See ook-demod for a description of these common constants

#define RF_FREQUENCY_MHZ 433.92

#define ONE_SECOND_US (1000 * 1000)
#define OREGON_CHIPRATE (1024  * 2)

#define ESTIMATED_TRIGGER_RSSI_DB -90

int main() {
    pinMode(LOGIC_TRIGGER, OUTPUT);
    digitalWrite(LOGIC_TRIGGER, LOW);

    stdio_init_all();

    Rfm69Common rfm69;
    rfm69.setPins(RFM69_MISO, RFM69_MOSI, RFM69_SCK, RFM69_CS, RFM69_IRQ, RFM69_RST);
    rfm69.begin(RF_FREQUENCY_MHZ);

    // Setup sampler PIO program on DIO2
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &sample_program);
    uint sm = pio_claim_unused_sm(pio, true);
    sample_program_init(pio, sm, offset, RFM69_DIO2);

    printf("Start...\n");
    while (true) {
        // get word from fifo if ready
        // we have rigged things this should be approximately twice a second
        int r = 0;
        uint32_t wrf = 0;
        uint32_t tt = 0;
        while (!pio_sm_is_rx_fifo_empty(pio, sm)) {
            if (!tt) {
                tt = to_ms_since_boot(get_absolute_time());
                // in theory this is 0.5 seconds apart
                // there will be some latency, but that should be consistent and thus the difference
                // between successive prints should cancel that out
                // in practice it seems to be exactly 516 msec...
                // an error of 128 pio cycles @ this divider
            }
            wrf = pio_sm_get(pio, sm);
            r++;
        }
        if (r > 0) {
            printf("Read %d words from Fifo. w[latest]=0x%.08x @ %d\n", r, wrf, tt);
        }
    }
    return 0;
}
