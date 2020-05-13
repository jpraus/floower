#ifndef FLOWER_H
#define FLOWER_H

#include "Arduino.h"
#include <ESP32Servo.h>
#include <NeoPixelAnimator.h>
#include "pixels.h"

class Flower {
  public:
    Flower();
    void init(int closedAngle, int openAngle);
    void update();

    void setPetalsOpenLevel(byte level, int transitionTime);

  private:
    bool setServoPowerOn(boolean powerOn);
    bool setPixelsPowerOn(boolean powerOn);

    NeoPixelAnimator animations; // animation management object used for both servo and pixels to animate
    void servoAnimationUpdate(const AnimationParam& param);

    // servo config
    Servo servo;
    int servoOpenAngle;
    int servoClosedAngle;

    // servo state
    byte petalsOpenLevel; // 0-100% (target angle in percentage)
    int servoAngle; // current angle
    int servoOriginAngle; // angle before animation
    int servoTargetAngle; // angle after animation
    bool servoPowerOn;

    // leds
    bool pixelsPowerOn = false;
};

#endif
