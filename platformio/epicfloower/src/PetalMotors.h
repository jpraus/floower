#pragma once

#include "Arduino.h"
#include <NeoPixelAnimator.h>
#include <PCF8575.h>

#define DIRECTION_OPEN 1
#define DIRECTION_CLOSE 0

typedef struct Motor {
    uint8_t pinEnable;
    uint8_t pwmChannel;
    uint8_t portIn1;    
    uint8_t portIn2;
    uint8_t direction;
    uint8_t acceleration;
    uint8_t dutyCycle;
    uint16_t durationMillis;
} Motor;

class PetalMotors {
    public:
        PetalMotors();
        void setup();
        void update();
        void runMotor(uint8_t motorIndex, uint8_t direction, uint16_t durationMillis, uint8_t acceleration);
        void stopMotor(uint8_t motorIndex);

    private:
        PCF8575 motorMultiplexor;
        Motor motors[6];
        NeoPixelAnimator animations; // animation management object used for motors acceleration/deceleration

        void setupMotor(uint8_t motorIndex, uint8_t pinEnable, uint8_t portIn1, uint8_t portIn2);
        void motorAnimationUpdate(const AnimationParam& param);
};