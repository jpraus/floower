#ifndef FLOOWER_H
#define FLOOWER_H

#include "Arduino.h"
#include "config.h"
#include <functional>
#include <NeoPixelBus.h>
#include <ESP32Servo.h>
#include <NeoPixelAnimator.h>

enum FloowerColorAnimation {
  RAINBOW,
  CANDLE
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
typedef std::function<void(const uint8_t petalsOpenLevel, const HsbColor color)> FloowerChangeCallback;

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
    void transitionColor(double hue, double saturation, double brightness, int transitionTime = 0);
    void transitionColorBrightness(double brightness, int transitionTime = 0);
    void flashColor(double hue, double saturation, int flashDuration);
    HsbColor getColor();
    HsbColor getCurrentColor();
    void startAnimation(FloowerColorAnimation animation);
    void stopAnimation(bool retainColor);
    bool isLit();
    bool isAnimating();
    bool arePetalsMoving();
    bool isChangingColor();

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
    void pixelsCandleAnimationUpdate(const AnimationParam& param);
    void showColor(HsbColor color);

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
    HsbColor pixelsColor; // current color
    HsbColor pixelsOriginColor; // color before animation
    HsbColor pixelsTargetColor; // color after animation
    bool pixelsPowerOn;

    // leds animations
    bool interruptiblePixelsAnimation = false;
    HsbColor candleOriginColors[6];
    HsbColor candleTargetColors[6];

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
