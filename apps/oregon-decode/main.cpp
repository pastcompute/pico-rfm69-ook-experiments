// This program tries to use IRQ pulse interval detection to decode oregon
// I'm using a decoder I found elsewhere on Github for this demo
// That being https://github.com/sfrwmaker/WirelessOregonV2

#include <Arduino.h>
#include <stdio.h>
#include <pico/stdlib.h>
#include "../rfm69common.h"
#include "DecodeOOK.h"
#include "OregonDecoderV2.h"

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

int sum(uint8_t count, const byte* buffer) {
    int s = 0;
 
    for(uint8_t i = 0; i < count; i++) {
        s += (buffer[i]&0xF0) >> 4;
        s += (buffer[i]&0xF);
    }
 
    if(int(count) != count)
        s += (buffer[count]&0xF0) >> 4;
 
    return s;
}

bool isSummOK(uint16_t sensorType, const byte* data, int len) {
    uint8_t s1 = 0;
    uint8_t s2 = 0;

    switch (sensorType) {
        case 0x1A2D:                                // THGR2228N
            s1 = (sum(8, data) - 0xa) & 0xFF;
            return (data[8] == s1);
        case 0xEA4C:                                // TNHN132N
            s1 = (sum(6, data) + (data[6]&0xF) - 0xa) & 0xff;
            s2 = (s1 & 0xF0) >> 4;
            s1 = (s1 & 0x0F) << 4;
            return ((s1 == data[6]) && (s2 == data[7]));
        default:
            break;
    }
    return false;
}

bool decodeTempHumidity(const byte* data, int len, uint8_t& channel, uint8_t& sensorId, int16_t& temp, uint8_t& hum, bool& battOK) {

    bool is_summ_ok = false;
    if (len >= 8) {
        uint16_t Type = (data[0] << 8) | data[1];
        is_summ_ok = isSummOK(Type, data, len);
        if (is_summ_ok) {
            int16_t t = data[5] >> 4;                   // 1st decimal digit
            t *= 10;
            t += data[5] & 0x0F;                        // 2nd decimal digit
            t *= 10;
            t += data[4] >> 4;                          // 3rd decimal digit
            if (data[6] & 0x08) t *= -1;
            temp = t;
            hum = 0;
            battOK = !(data[4] & 0x0C);
            // 1a2D shows as 1d20 in Pulseview... for the exact same data...
            if (Type == 0x1A2D) {                       // THGR228N, THGN123N, THGR122NX, THGN123N
                hum  = data[7] & 0xF;
                hum *= 10;
                hum += data[6] >> 4;
            }
        }
    }
    return is_summ_ok;
}

int main() {
    pinMode(LOGIC_TRIGGER, OUTPUT);
    digitalWrite(LOGIC_TRIGGER, LOW);

    stdio_init_all();

    pinMode(RFM69_DIO2, INPUT_PULLDOWN);

    Rfm69Common rfm69;
    rfm69.setPins(RFM69_MISO, RFM69_MOSI, RFM69_SCK, RFM69_CS, RFM69_IRQ, RFM69_RST);
    rfm69.begin(RF_FREQUENCY_MHZ);

    absolute_time_t tNow = get_absolute_time();
    absolute_time_t tNextSecond = delayed_by_us(tNow, ONE_SECOND_US);
    int n=0;
    

    printf("Start decoding...\n");
    OregonDecoderV2 orscV2;
    extern void reportSerial (const char* s, class DecodeOOK& decoder);


    // Setup interrupts on DIO2
    // Because we are expecting manchester encoding, we want to trigger both rising and falling edges
    extern void dio2InterruptHandler();
    sharedData.edgesCount = 0;
    sharedData.nextPulseLength_us = 0;
    attachInterrupt(digitalPinToInterrupt(RFM69_DIO2), dio2InterruptHandler, CHANGE);

    auto t0 = to_ms_since_boot(get_absolute_time());
    uint8_t channel;
    uint8_t sensorID;
    int16_t temp;
    uint8_t hum;
    bool battOK;
    bool latchPreamble = false;
    bool preambleValue = false;
    while (true) {
        noInterrupts();
        auto pulseLength_us = sharedData.nextPulseLength_us;
        sharedData.nextPulseLength_us = 0;
        auto now_us = sharedData.now;
        interrupts();
        if (pulseLength_us != 0) {
            if (orscV2.nextPulse(pulseLength_us)) {
                printf("%d ", n);
                reportSerial("OSV2", orscV2);
                uint8_t len;
                const byte* data = orscV2.getData(len);
                if (data) {
                    if (decodeTempHumidity(data, len, channel, sensorID, temp, hum, battOK)) {
                        printf("%d,%d,%d,%d,%d,%d\n", n, channel, sensorID, temp, hum, battOK);
                    }
                }
                orscV2.resetDecoder();
            }
        }


        if (time_reached(tNextSecond)) {
            auto t1 = to_ms_since_boot(get_absolute_time());
            tNow = get_absolute_time();
            tNextSecond = delayed_by_us(tNow, ONE_SECOND_US);
            printf((n % 2 == 0) ? "- %d    \r" : "| %d    \r", (t1 - t0)/1000);
            sleep_ms(1);
            n++;
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

void reportSerial (const char* s, class DecodeOOK& decoder) {
    byte pos;
    const byte* data = decoder.getData(pos);
    Serial.print(s);
    Serial.print(' ');
    for (byte i = 0; i < pos; ++i) {
        Serial.print(data[i] >> 4, HEX);
        Serial.print(data[i] & 0x0F, HEX);
    }
    
    // Serial.print(' ');
    // Serial.print(millis() / 1000);
    Serial.println();
}

// OK. THis is buggy, it misses every second
// e.g. 78s cadence, not 39
// The Wless version was rubbish but it at least found a preabmle every 39 s