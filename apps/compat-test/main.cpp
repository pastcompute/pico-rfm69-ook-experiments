#include "Arduino.h"
#include "pico/stdlib.h"

#include "elapsedMillis.h"

int main() {
    stdio_init_all(); // needed!
    printf("printf\n");
    Serial.println("serial.println");
    printf("printf\n");

    elapsedMillis now;
    printf("now=%d\n", now.get());

    delay(5000);

    printf("now=%d\n", (long)now);

    return 0; 
}
