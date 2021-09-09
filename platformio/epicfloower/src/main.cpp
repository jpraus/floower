#include <Arduino.h>
#include "PetalMotors.h"

PetalMotors petalMotors;

uint8_t direction = DIRECTION_CLOSE;
unsigned long timer = 0;

void setup() {
    Serial.begin(115200);
    petalMotors.setup();

    petalMotors.runMotor(0, direction, 10000, 10);
    petalMotors.runMotor(1, direction, 10000, 10);
    timer = millis() + 11000;
}

void loop() {
    petalMotors.update();

    /*if (timer <= millis()) {
        if (direction == DIRECTION_CLOSE) {
            direction = DIRECTION_OPEN;
            petalMotors.runMotor(0, direction, 5000, 10);
            petalMotors.runMotor(1, direction, 5000, 10);
            timer = millis() + 6000;
        }
        else {
            direction = DIRECTION_CLOSE;
            petalMotors.runMotor(0, direction, 7000, 10);
            petalMotors.runMotor(1, direction, 7000, 10);
            timer = millis() + 7000;
        }
    }*/
}