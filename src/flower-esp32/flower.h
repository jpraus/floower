#ifndef FLOWER_H
#define FLOWER_H

#include "Arduino.h"
#include <ESP32Servo.h>
#include <NeoPixelAnimator.h>
#include "pixels.h"

class Flower {
  public:
    Flower();
    void init(int closedAngle);
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
    bool servoPowerOn;
    int servoAngle;
    int servoTargetAngle;
    byte servoDir;
    byte petalsOpenLevel; // 0-100%

    // leds
    bool pixelsPowerOn = false;
};

#endif
