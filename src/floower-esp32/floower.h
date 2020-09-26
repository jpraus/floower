#ifndef FLOOWER_H
#define FLOOWER_H

#include "Arduino.h"
#include "config.h"
#include <functional>
#include <NeoPixelBus.h>
#include <ESP32Servo.h>
#include <NeoPixelAnimator.h>

enum FloowerColorMode {
  TRANSITION,
  FLASH
};

enum FloowerTouchEvent {
  TOUCH_DOWN,
  TOUCH_LONG, // >2s
  TOUCH_HOLD, // >5s
  TOUCH_UP
};

struct Battery {
  float voltage;
  byte level;
};

typedef std::function<void(const FloowerTouchEvent& event)> FloowerOnLeafTouchCallback;

class Floower {
  public:
    Floower(Config *config);
    void init();
    void initServo();
    void update();

    void registerOutsideTouch();
    void enableTouch();
    void onLeafTouch(FloowerOnLeafTouchCallback callback);

    void setPetalsOpenLevel(byte level, int transitionTime = 0);
    byte getPetalOpenLevel();
    void setColor(RgbColor color, FloowerColorMode colorMode, int transitionTime = 0);
    RgbColor getColor();
    void startRainbow();
    void stopRainbowRetainColor();
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
    void pixelsFlashAnimationUpdate(const AnimationParam& param);
    void pixelsRainbowAnimationUpdate(const AnimationParam& param);
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
    FloowerOnLeafTouchCallback touchCallback;
    static unsigned long touchStartedTime;
    static unsigned long touchEndedTime;
    static unsigned long lastTouchTime;
    bool touchRegistered = false;
    bool holdTouchRegistered = false;
    bool longTouchRegistered = false;

    // battery
    Battery batteryState;
    bool lowPowerMode;

    // acty
    unsigned long actyOffTime;
};

#endif
