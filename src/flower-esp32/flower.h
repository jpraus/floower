#ifndef FLOWER_H
#define FLOWER_H

#include "Arduino.h"
#include <ESP32Servo.h>
#include <NeoPixelAnimator.h>
#include "pixels.h"

const RgbColor colorRed(128, 2, 0);
const RgbColor colorGreen(0, 128, 0);
const RgbColor colorBlue(0, 20, 128);
const RgbColor colorYellow(128, 70, 0);
const RgbColor colorOrange(128, 30, 0);
const RgbColor colorWhite(128);
const RgbColor colorPurple(128, 0, 128);
const RgbColor colorPink(128, 0, 50);
const RgbColor colorBlack(0);

class Flower {
  public:
    Flower();
    void init(int closedAngle, int openAngle, byte ledsModel);
    void update();

    void setPetalsOpenLevel(byte level, int transitionTime = 0);
    void setColor(RgbColor color, int transitionTime = 0);
    bool isAnimating();
    bool isIdle();

    float readBatteryVoltage();
    void acty();

    void onLeafTouch(void (*callback)());

  private:
    bool setServoPowerOn(boolean powerOn);
    bool setPixelsPowerOn(boolean powerOn);

    NeoPixelAnimator animations; // animation management object used for both servo and pixels to animate
    void servoAnimationUpdate(const AnimationParam& param);
    void pixelsAnimationUpdate(const AnimationParam& param);

    void handleTimers();

    // servo config
    Servo servo;
    int servoOpenAngle;
    int servoClosedAngle;

    // servo state
    int petalsOpenLevel; // 0-100% (target angle in percentage)
    int servoAngle; // current angle
    int servoOriginAngle; // angle before animation
    int servoTargetAngle; // angle after animation
    bool servoPowerOn;
    long servoPowerOffTime; // time when servo should power off (after animation is finished)

    // leds
    Pixels pixels;

    // leds state
    RgbColor pixelsColor; // current color
    RgbColor pixelsOriginColor; // color before animation
    RgbColor pixelsTargetColor; // color after animation
    bool pixelsPowerOn;

    // touch
    void (*touchCallback)();

    // acty
    long actyOffTime;
};

#endif
