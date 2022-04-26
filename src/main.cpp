#include <Arduino.h>
#include "rs485.h"


Rs485 ser(PC5, PC4);

void setup() {
    pinMode(LED_ORANGE, OUTPUT);
    // digitalWrite(LED_ORANGE, HIGH);

    ser.begin(115200);
}

void loop() {
    ser.println("LED on?! no :|");
}