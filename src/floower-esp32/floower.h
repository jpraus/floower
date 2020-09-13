#ifndef FLOOWER_H
#define FLOOWER_H

#include "Arduino.h"
#include <ESP32Servo.h>
#include <NeoPixelAnimator.h>
#include "pixels.h"

const RgbColor colorRed(127, 2, 0);
const RgbColor colorGreen(0, 127, 0);
const RgbColor colorBlue(0, 20, 127);
const RgbColor colorYellow(127, 70, 0);
const RgbColor colorOrange(127, 30, 0);
const RgbColor colorWhite(127);
const RgbColor colorPurple(127, 0, 127);
const RgbColor colorPink(127, 0, 50);
const RgbColor colorBlack(0);

enum FloowerColorMode {
  TRANSITION,
  PULSE
};

enum FloowerTouchType {
  TOUCH,
  HOLD,
  RELEASE
};

struct Battery {
  float voltage;
  byte level;
};

class Floower {
  public:
    Floower();
    void init(byte ledsModel);
    void initServo(int closedAngle, int openAngle);
    void update();

    void touchISR();
    void static touchAttachInterruptProxy(void (*callback)()); // Raw touch interrupt. Need to be relayed to touchISR() due to limittation of touchAttachInterrupt function
    void onLeafTouch(void (*callback)(FloowerTouchType type));

    void setPetalsOpenLevel(byte level, int transitionTime = 0);
    byte getPetalOpenLevel();
    void setColor(RgbColor color, FloowerColorMode colorMode, int transitionTime = 0);
    RgbColor getColor();
    void startColorPicker();
    void stopColorPicker();
    bool arePetalsMoving();
    bool isIdle();

    void acty();
    Battery readBatteryState();
    bool isUSBPowered();
    void setLowPowerMode(boolean lowPowerMode);
    bool isLowPowerMode();

  private:
    typedef void (*voidFnPtr)();
    voidFnPtr CallBackFnPtr;
  
    bool setServoPowerOn(boolean powerOn);
    bool setPixelsPowerOn(boolean powerOn);

    NeoPixelAnimator animations; // animation management object used for both servo and pixels to animate
    void servoAnimationUpdate(const AnimationParam& param);
    void pixelsTransitionAnimationUpdate(const AnimationParam& param);
    void pixelsPulseAnimationUpdate(const AnimationParam& param);
    void colorPickerAnimationUpdate(const AnimationParam& param);
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
    FloowerColorMode pixelsColorMode;
    bool pixelsPowerOn;

    // touch
    void (*touchCallback)(FloowerTouchType type);
    unsigned long touchStartedTime = 0;
    unsigned long touchEndedTime = 0;
    unsigned long lastTouchTime = 0;
    bool touchRegistered = false;
    bool holdTouchRegistered = false;

    // battery
    Battery batteryState;
    bool lowPowerMode;

    // acty
    unsigned long actyOffTime;
};

#endif
