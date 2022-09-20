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

bool decodeTempHumidity(const byte* data, int len, uint16_t& actualType, uint8_t& channel, uint8_t& rollingCode, int16_t& temp, uint8_t& hum, bool& battOK) {

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
            channel = data[2] >> 4;
            rollingCode = 
                         ((data[3] & 0xf) << 4) |     // 2
                         ((data[3] >> 4));            // 7

            actualType = (uint16_t(data[0] >> 4) << 12) |     // 1
                         (uint16_t(data[1] & 0xf) << 8) |     // d
                         (uint16_t(data[1] >> 4) << 4)  |     // 2
                         (uint16_t(data[2]) & 0xf);           // 0
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
    uint8_t rollingCode;
    int16_t temp;
    uint8_t hum;
    uint16_t actualType = 0;
    bool battOK;
    bool latchPreamble = false;
    bool preambleValue = false;
    bool needToPrint = false;
    float rssi;
    while (true) {
        noInterrupts();
        auto pulseLength_us = sharedData.nextPulseLength_us;
        sharedData.nextPulseLength_us = 0;
        auto now_us = sharedData.now;
        interrupts();
        if (pulseLength_us != 0) {
            bool decoded = false;
            if (orscV2.nextPulse(pulseLength_us)) {
                rssi = rfm69.readRSSIByte() / -2.0F; // even though this is just after the message it seems to be pretty right
                printf("%d ", n);
                reportSerial("OSV2", orscV2);
                uint8_t len;
                const byte* data = orscV2.getData(len);
                if (data) {
                    // Rolling code is BCD...
                    if (decodeTempHumidity(data, len, actualType, channel, rollingCode, temp, hum, battOK)) {
                        decoded = true;
                    }
                }
                orscV2.resetDecoder();
            }
            if (decoded) {
                printf("%d,%04x,%d,%x,%.1f,%d,Batt=%s,%.1fdB\n", n, actualType, channel, rollingCode, temp / 10.F, hum, battOK?"ok":"flat", rssi);
            }
        }

        if (time_reached(tNextSecond)) {
            auto t1 = to_ms_since_boot(get_absolute_time());
            tNow = get_absolute_time();
            tNextSecond = delayed_by_us(tNow, ONE_SECOND_US);
#if 1 // in case this is interfering with timing...
            rssi = rfm69.readRSSIByte() / -2.0F;
            printf((n % 2 == 0) ? "- %d %.1f    \r" : "| %d %.1f    \r", (t1 - t0)/1000, rssi);
            sleep_ms(1);
#endif
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

    // So for V2 protocol, only every second bit actually counts
    // Which means directly comparing the bytes the way they have been
    // extracted here, against what we see in Pulseview, doesn't work
    // pulseview visualisation sync shows:
    // Fields (hex) 32 d4 cb 35 54 d5 34 ad 55 52 cc d5 55 4c ad 4a aa b5 2a (19)
    // L2     (hex) 96 a5 59 aa a6 a9 a5 6a aa 96 66 aa aa 65 6a 55 55 a9 56
    // L2 +4  (hex) 6a 65 9a aa ... (these correctly decode using bitpairs)
    // Where L2 is shifted 4 bits further compared with 'Fields'
    // Yet these hex numbers in Pulseview are also offset by what looked like 1-bit to the bytes
    // bit this was fixed by using a +4 offset in the visualiser
    // from which the nibbles that decode to temperature and humidity are extracted:
    //
    // So, the OOK "visualsation" in Pulseview is off by one bit position for Oregon
    // until we fix the sync offset --> 4
    //
    // SensorId 0110 1010 0110 0101 1001 1010 1010 1010 --> 1d20
    //          6    a    6    5    9    a    a    a
    //          0 1  1 1  0 1  0 0  1 0  1 1  1 1  1 1
    //          1 0  0 0  1 0  1 1  0 1  0 0  0 0  0 0 
    //          0 0  0 1  1 1  0 1  0 0  1 0  0 0  0 0 (flipped) 
    //          1         d         2        0 (as pulseview decoded)
    //
    // Channel  0110 1010  --> 1
    // Temp     1010 1001 0110 0110 0110 1010 --> 8 5 1 == 15.8 degrees
    //          a    9    6    6    6    a
    //          1 1  1 0  0 1  0 1  0 1  1 1  --> as bit pair decoding
    //          0 0  0 1  1 0  1 0  1 0  0 0  --> flip 1 0 0 0  0 1 0 1   0 0 0 1 --> 8 5 1
    //          
    // of these, 
    // here we are printing something like this, which is after bit-pair removal
    // (hex) 1A 2D, 10, 72, 30, 14 10, C7, 36 B7 (10)
    // temp:   0001 0100 0001 0000 --> 1000 0010 1000 0000    8 4 1 0 --> 14.8 degrees
    // humid:  1100 1110 --> 0011 0111 --> 3 7 ---> 73%           
    //
    // convert these back to bitpairs and reverse the order and we match what is in Pulseview with a +4 visualiser
    //
    //
    // So this code computed humid/temp (and checksum) correctm but not the ID
    // Id:  1a2d -->
    //         0001 1010 0010 1101 --> 1000 0101 0100 1011 --> 8 5 4 b ?
    // Or has it not extract the bit pairs correctly?
    // Actually it is offset and swapped nibbles?
    // Pulseview:  x x x x  0 0 0 1  1 1 0 1  0 0 1 0  0 0 0 0  y y y y
    // Here:                0 0 0 1  1 0 1 0  0 0 1 0  1 1 0 1
    //
    // Actually after looking at the decoder code again, it gels with everything being offset by 4 bits...
    // in the dump:
    // the 2nd-leftmost nibble is the sync,
    // the leftmost is the 1 from 1d20 etc


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