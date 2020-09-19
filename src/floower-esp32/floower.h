#ifndef FLOOWER_H
#define FLOOWER_H

#include "Arduino.h"
#include "config.h"
#include <NeoPixelBus.h>
#include <ESP32Servo.h>
#include <NeoPixelAnimator.h>

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
    Floower(Config *config);
    void init();
    void initServo();
    void update();

    void registerOutsideTouch();
    void enableTouch();
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
    bool setServoPowerOn(boolean powerOn);
    bool setPixelsPowerOn(boolean powerOn);

    NeoPixelAnimator animations; // animation management object used for both servo and pixels to animate
    void servoAnimationUpdate(const AnimationParam& param);
    void pixelsTransitionAnimationUpdate(const AnimationParam& param);
    void pixelsPulseAnimationUpdate(const AnimationParam& param);
    void colorPickerAnimationUpdate(const AnimationParam& param);
    void showColor(RgbColor color);

    void handleTimers();
    static void touchISR();

    Config *config;

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
    NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> pixels;

    // leds state
    RgbColor pixelsColor; // current color
    RgbColor pixelsOriginColor; // color before animation
    RgbColor pixelsTargetColor; // color after animation
    FloowerColorMode pixelsColorMode;
    bool pixelsPowerOn;

    // touch
    void (*touchCallback)(FloowerTouchType type);
    static unsigned long touchStartedTime;
    static unsigned long touchEndedTime;
    static unsigned long lastTouchTime;
    bool touchRegistered = false;
    bool holdTouchRegistered = false;

    // battery
    Battery batteryState;
    bool lowPowerMode;

    // acty
    unsigned long actyOffTime;
};

#endif
