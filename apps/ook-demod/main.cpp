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
#include <RH_RF69.h>
#include <RHSoftwareSPI.h>

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

#define RFM69_DIO2 D18

#define RF_FREQUENCY_MHZ 433.92

#define ONE_SECOND_US (1000 * 1000)
#define OREGON_CHIPRATE (1024  * 2)
#define FXOSC 32000000

// This is the RSSI to use to trigger the logic analyser
#define ESTIMATED_TRIGGER_RSSI_DB -90

#define OOK_USE_FIXED_PEAK_DETECTOR false
#define OOK_FIXED_PEAK_DETECT_THRESHOLD_DB 21

int main() {
    // Trigger for the logic analyser, corresponds to the first pulse in RSSI
    pinMode(LOGIC_TRIGGER, OUTPUT);
    digitalWrite(LOGIC_TRIGGER, LOW);

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

    // Tune the receiver
    rf69module.setFrequency(RF_FREQUENCY_MHZ);

    // Configure the modem
    // Note, RadioHead has a function for this where you create a register structure
    // and it can leverage bulk SPI write, but it only has a subset, and also wrties registers we dont even need
    // Given we want to write other registers we may as well do all of them directly
    // We could even have done the frequency, but the library for convenience
    // converts MHz to the necessary bytes so we leave that be

    // By doing it ourselves we can easier check against the SX1231 manual what is happening
    // And document it here...

    // Enable continuous OOK mode without bit synchronisation,
    // because we are trying to receive OOK transmissions from anywhere
    #define MODEM_CONFIG_OOK_CONT_NO_SYNC (\
        RH_RF69_DATAMODUL_DATAMODE_CONT_WITHOUT_SYNC | \
        RH_RF69_DATAMODUL_MODULATIONTYPE_OOK | \
        RH_RF69_DATAMODUL_MODULATIONSHAPING_OOK_NONE)

    // Set the bandwidth to 100kHz with 1% DC cancellation
    // See SX1231 manual - Channel Filter - pages ~27,28
    #define MODEM_CONFIG_BW_100k_DCC_1 0x89

    // Set the bit rate to correspond to an Oregon V2/V3 protocol transmitter
    // Although it was not obvious from the manual, this cleans up noise that remains otherwise
    // For OOK this is the chip rate so 2x the bitrate
    // Without setting the bitrate, the OOK decoder in Pulseview fails to work
    // and you can see significant noise on each bit
    #define OOK_BITRATE OREGON_CHIPRATE
    const byte brLSB = (FXOSC / OOK_BITRATE) & 0xff;
    const byte brMSB = ((FXOSC / OOK_BITRATE) >> 8) & 0xff;    

    rf69module.spiWrite(RH_RF69_REG_02_DATAMODUL, MODEM_CONFIG_OOK_CONT_NO_SYNC);
    rf69module.spiWrite(RH_RF69_REG_03_BITRATEMSB, brMSB);
    rf69module.spiWrite(RH_RF69_REG_04_BITRATELSB, brLSB);
    rf69module.spiWrite(RH_RF69_REG_19_RXBW, MODEM_CONFIG_BW_100k_DCC_1);

    // To help calibrate our logic analyser, output a 1MHz frequency on DIO5 (This is FXOSC/32)
    // Also the RSSI state on DIO0  + OOK on DIO2 ( which is the same for all values in continous)
    // Map1 register: DIO 3-2-1-0 (LSB --> MSB pairs)
    // Map2 register: DIO 5-4 (high nibble), fxosc (low 3)
    // See Table21 in the SX1231 manual
    byte dmap1 = rf69module.spiRead(RH_RF69_REG_25_DIOMAPPING1);
    dmap1 = (dmap1 & 0xfc) | 2; // DIO0: bits 0-1 --> 10 == RSSI
    rf69module.spiWrite(RH_RF69_REG_25_DIOMAPPING1, dmap1);

    byte dmap2 = rf69module.spiRead(RH_RF69_REG_26_DIOMAPPING2);
    dmap2 = (dmap2 & 0x38) | 5; // Clock out frequency bits 0-2 --> 101, DIO5 bits 7-6 --> 00 == clock 
    rf69module.spiWrite(RH_RF69_REG_26_DIOMAPPING2, dmap2);

    // With a good guess of the RSSI threshold value ESTIMATED_TRIGGER_RSSI_DB
    // it is not necessary to use the peak detector
    // However using the peak detector will eliminate junk beyond the valid transmissions
    // as well as right nearby
    if (OOK_USE_FIXED_PEAK_DETECTOR) {
        printf("ASK threshold is fixed to %ddB above the floor\n", OOK_FIXED_PEAK_DETECT_THRESHOLD_DB);
        rf69module.spiWrite(RH_RF69_REG_1B_OOKPEAK, 0); // bits 6-7 default 0x40 (peak), 00 (fixed), 10 (av)
        rf69module.spiWrite(RH_RF69_REG_1D_OOKFIX, OOK_FIXED_PEAK_DETECT_THRESHOLD_DB);
    } else {
        printf("ASK threshold is relative to background RSSI\n");
    }

    // Read back the current operating mode & confirm we set the data mode succesfully
    byte opMode = rf69module.spiRead(RH_RF69_REG_01_OPMODE);
    byte datMode = rf69module.spiRead(RH_RF69_REG_02_DATAMODUL);
    byte map1 = rf69module.spiRead(RH_RF69_REG_25_DIOMAPPING1);
    byte map2 = rf69module.spiRead(RH_RF69_REG_26_DIOMAPPING2);

    // Note, DIO2 is always OOK out in Continuous mode
    printf("Actual OPMODE=%02x DATAMOD=%02x DIOMAP=%02x %02x\n", opMode, datMode, map1, map2);

    // Setup to output demodulated OOK on DIO2
    pinMode(RFM69_DIO2, INPUT_PULLDOWN);

    printf("Start receiving.\n");
    rf69module.setModeRx();

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
        rssi = rf69module.spiRead(RH_RF69_REG_24_RSSIVALUE);

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
