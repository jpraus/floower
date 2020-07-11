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

enum FlowerColorMode {
  TRANSITION,
  PULSE
};

class Flower {
  public:
    Flower();
    void init(byte ledsModel);
    void initServo(int closedAngle, int openAngle);
    void update();

    void setPetalsOpenLevel(byte level, int transitionTime = 0);
    void setColor(RgbColor color, FlowerColorMode colorMode, int transitionTime = 0);
    bool isAnimating();
    bool isIdle();

    void acty();
    float readBatteryVoltage();
    bool isCharging();
    bool isUSBPowered();
    void setLowPowerMode(boolean lowPowerMode);
    bool isLowPowerMode();

    void onLeafTouch(void (*callback)());

  private:
    bool setServoPowerOn(boolean powerOn);
    bool setPixelsPowerOn(boolean powerOn);

    NeoPixelAnimator animations; // animation management object used for both servo and pixels to animate
    void servoAnimationUpdate(const AnimationParam& param);
    void pixelsTransitionAnimationUpdate(const AnimationParam& param);
    void pixelsPulseAnimationUpdate(const AnimationParam& param);
    void showColor(RgbColor color);

    void handleTimers();

    // servo config
    Servo servo;
    unsigned int servoOpenAngle;
    unsigned int servoClosedAngle;

    // servo state
    signed int petalsOpenLevel; // 0-100% (target angle in percentage)
    signed int servoAngle; // current angle
    signed int servoOriginAngle; // angle before animation
    signed int servoTargetAngle; // angle after animation
    bool servoPowerOn;
    unsigned long servoPowerOffTime; // time when servo should power off (after animation is finished)

    // leds
    Pixels pixels;

    // leds state
    RgbColor pixelsColor; // current color
    RgbColor pixelsOriginColor; // color before animation
    RgbColor pixelsTargetColor; // color after animation
    FlowerColorMode pixelsColorMode;
    bool pixelsPowerOn;

    // touch
    void (*touchCallback)();

    // battery
    float batteryVoltage;
    bool lowPowerMode;

    // acty
    unsigned long actyOffTime;
};

#endif
