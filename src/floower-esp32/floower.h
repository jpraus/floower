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
  uint8_t level;
};

typedef std::function<void(const FloowerTouchEvent& event)> FloowerOnLeafTouchCallback;
typedef std::function<void(const uint8_t petalsOpenLevel, const RgbColor color)> FloowerChangeCallback;

class Floower {
  public:
    Floower(Config *config);
    void init();
    void initServo();
    void update();

    void registerOutsideTouch();
    void enableTouch();
    void onLeafTouch(FloowerOnLeafTouchCallback callback);
    void onChange(FloowerChangeCallback callback);

    void setPetalsOpenLevel(uint8_t level, int transitionTime = 0);
    void setPetalsAngle(unsigned int angle, int transitionTime = 0);
    uint8_t getPetalsOpenLevel();
    uint8_t getCurrentPetalsOpenLevel();
    int getPetalsAngle();
    int getCurrentPetalsAngle(); 
    void setColor(RgbColor color, FloowerColorMode colorMode, int transitionTime = 0);
    RgbColor getColor();
    void startRainbow();
    void stopRainbowRetainColor();
    bool arePetalsMoving();
    bool isLit();
    bool isIdle();

    void acty();
    Battery readBatteryState();
    bool isUSBPowered();
    void setLowPowerMode(bool lowPowerMode);
    bool isLowPowerMode();

  private:
    bool setServoPowerOn(bool powerOn);
    bool setPixelsPowerOn(bool powerOn);

    NeoPixelAnimator animations; // animation management object used for both servo and pixels to animate
    void servoAnimationUpdate(const AnimationParam& param);
    void pixelsTransitionAnimationUpdate(const AnimationParam& param);
    void pixelsFlashAnimationUpdate(const AnimationParam& param);
    void pixelsRainbowAnimationUpdate(const AnimationParam& param);
    void showColor(RgbColor color);

    void handleTimers(unsigned long now);
    static void touchISR();

    Config *config;
    FloowerChangeCallback changeCallback;

    // servo config
    Servo servo;
    unsigned int servoOpenAngle;
    unsigned int servoClosedAngle;

    // servo state
    int8_t petalsOpenLevel; // 0-100% (target angle in percentage)
    signed int servoAngle; // current angle
    signed int servoOriginAngle; // angle before animation
    signed int servoTargetAngle; // angle after animation
    bool servoPowerOn;
    unsigned long servoPowerOffTime; // time when servo should power off (after animation is finished)

    // leds
    NeoPixelBus<NeoGrbFeature, NeoEsp32I2s0800KbpsMethod> pixels;

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
