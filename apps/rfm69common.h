#ifndef APPS_RFM69_COMMON
#define APPS_RFM69_COMMON

#include <Arduino.h>
#include <stdio.h>
#include <pico/stdlib.h>
#include <RH_RF69.h>
#include <RHSoftwareSPI.h>

#include <memory>

#define FXOSC 32000000

#define OREGON_CHIPRATE (1024  * 2)

#define OOK_USE_FIXED_PEAK_DETECTOR false
#define OOK_FIXED_PEAK_DETECT_THRESHOLD_DB 21

class Rfm69Common {
private:
    uint8_t pin_miso;
    uint8_t pin_mosi;
    uint8_t pin_sck;
    uint8_t pin_cs;
    uint8_t pin_irq;
    uint8_t pin_rst;

    // This is hacky but it lets us avoid having any RH code in main
    std::unique_ptr<RHSoftwareSPI> spi;
    std::unique_ptr<RH_RF69> rfm69module;

public:
    Rfm69Common() {}

    void setPins(uint8_t miso, uint8_t mosi, uint8_t sck, uint8_t cs, uint8_t irq, uint8_t rst) {
      pin_miso = miso;
      pin_mosi = mosi;
      pin_sck = sck;
      pin_cs = cs;
      pin_irq = irq;
      pin_rst = rst;
    }

    uint8_t readRSSIByte() const {
        return rfm69module->spiRead(RH_RF69_REG_24_RSSIVALUE);
    }

    void begin(float frequency) {
        spi.reset(new RHSoftwareSPI());
        spi->setPins(pin_miso, pin_mosi, pin_sck);

        rfm69module.reset(new RH_RF69(pin_cs, pin_irq, *spi));

        // Reset the module first.
        // From the SX1231 data sheet, pulse RST for 100 uS then wait at least 5 ms
        // We go a bit longer to make sure
        printf("SX1231 reset...\n");
        pinMode(pin_rst, OUTPUT);
        digitalWrite(pin_rst, HIGH); delay(10);
        digitalWrite(pin_rst, LOW); delay(10);
        if (!rfm69module->init()) {
            panic("Failed to initialise the RFM69 - probably this is a SPI problem");
        }

        // Tune the receiver
        rfm69module->setFrequency(frequency);

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
        const uint8_t MODEM_CONFIG_OOK_CONT_NO_SYNC =
            RH_RF69_DATAMODUL_DATAMODE_CONT_WITHOUT_SYNC |
            RH_RF69_DATAMODUL_MODULATIONTYPE_OOK |
            RH_RF69_DATAMODUL_MODULATIONSHAPING_OOK_NONE;

        // Set the bandwidth to 100kHz with 4% DC cancellation
        // See SX1231 manual - Channel Filter - pages ~27,28
        const uint8_t MODEM_CONFIG_BW_100k_DCC_1 = 0x49;

        // Set the bit rate to correspond to an Oregon V2/V3 protocol transmitter
        // Although it was not obvious from the manual, this cleans up noise that remains otherwise
        // For OOK this is the chip rate so 2x the bitrate
        // Without setting the bitrate, the OOK decoder in Pulseview fails to work
        // and you can see significant noise on each bit
        const uint32_t OOK_BITRATE = OREGON_CHIPRATE;
        const byte brLSB = (FXOSC / OOK_BITRATE) & 0xff;
        const byte brMSB = ((FXOSC / OOK_BITRATE) >> 8) & 0xff;    

        rfm69module->spiWrite(RH_RF69_REG_02_DATAMODUL, MODEM_CONFIG_OOK_CONT_NO_SYNC);
        rfm69module->spiWrite(RH_RF69_REG_03_BITRATEMSB, brMSB);
        rfm69module->spiWrite(RH_RF69_REG_04_BITRATELSB, brLSB);
        rfm69module->spiWrite(RH_RF69_REG_19_RXBW, MODEM_CONFIG_BW_100k_DCC_1);

        // To help calibrate our logic analyser, output a 1MHz frequency on DIO5 (This is FXOSC/32)
        // Also the RSSI state on DIO0  + OOK on DIO2 ( which is the same for all values in continous)
        // Map1 register: DIO 3-2-1-0 (LSB --> MSB pairs)
        // Map2 register: DIO 5-4 (high nibble), fxosc (low 3)
        // See Table21 in the SX1231 manual
        byte dmap1 = rfm69module->spiRead(RH_RF69_REG_25_DIOMAPPING1);
        dmap1 = (dmap1 & 0xfc) | 2; // DIO0: bits 0-1 --> 10 == RSSI
        rfm69module->spiWrite(RH_RF69_REG_25_DIOMAPPING1, dmap1);

        byte dmap2 = rfm69module->spiRead(RH_RF69_REG_26_DIOMAPPING2);
        dmap2 = (dmap2 & 0x38) | 5; // Clock out frequency bits 0-2 --> 101, DIO5 bits 7-6 --> 00 == clock 
        rfm69module->spiWrite(RH_RF69_REG_26_DIOMAPPING2, dmap2);

        // With a good guess of the RSSI threshold value ESTIMATED_TRIGGER_RSSI_DB
        // it is not necessary to use the peak detector
        // However using the peak detector will eliminate junk beyond the valid transmissions
        // as well as right nearby
        if (OOK_USE_FIXED_PEAK_DETECTOR) {
            printf("ASK threshold is fixed to %ddB above the floor\n", OOK_FIXED_PEAK_DETECT_THRESHOLD_DB);
            rfm69module->spiWrite(RH_RF69_REG_1B_OOKPEAK, 0); // bits 6-7 default 0x40 (peak), 00 (fixed), 10 (av)
            rfm69module->spiWrite(RH_RF69_REG_1D_OOKFIX, OOK_FIXED_PEAK_DETECT_THRESHOLD_DB);
        } else {
            printf("ASK threshold is relative to background RSSI\n");
        }

        // Read back the current operating mode & confirm we set the data mode succesfully
        byte opMode = rfm69module->spiRead(RH_RF69_REG_01_OPMODE);
        byte datMode = rfm69module->spiRead(RH_RF69_REG_02_DATAMODUL);
        byte map1 = rfm69module->spiRead(RH_RF69_REG_25_DIOMAPPING1);
        byte map2 = rfm69module->spiRead(RH_RF69_REG_26_DIOMAPPING2);

        // Note, DIO2 is always OOK out in Continuous mode
        printf("Actual OPMODE=%02x DATAMOD=%02x DIOMAP=%02x %02x\n", opMode, datMode, map1, map2);

        printf("Start receiving.\n");
        rfm69module->setModeRx();
    }
};

#endif
