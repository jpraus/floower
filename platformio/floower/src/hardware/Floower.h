#pragma once

#include "Arduino.h"
#include "Config.h"
#include "hardware/Petals.h"
#include <tmc2300.h>
#include <functional>
#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>
#include <AccelStepper.h>

enum FloowerColorAnimation {
    RAINBOW,
    RAINBOW_LOOP,
    CANDLE
};

enum FloowerStatusAnimation {
    STILL,
    BLINK_ONCE,
    PULSATING
};

enum FloowerTouchEvent {
    TOUCH_DOWN,
    TOUCH_LONG, // >2s
    TOUCH_HOLD, // >5s
    TOUCH_UP
};

struct PowerState {
    float batteryVoltage;
    uint8_t batteryLevel;
    bool batteryCharging;
    bool usbPowered;
    bool switchedOn;
};

typedef std::function<void(const FloowerTouchEvent& event)> FloowerOnLeafTouchCallback;
typedef std::function<void(const uint8_t petalsOpenLevel, const HsbColor color)> FloowerChangeCallback;

class Floower {
    public:
        Floower(Config *config);
        void init();
        void initPetals(unsigned long currentPosition = 0);
        void update();

        void registerOutsideTouch();
        void enableTouch(FloowerOnLeafTouchCallback callback, bool defer = false);
        void reconfigureTouch();
        void disableTouch();
        uint8_t readTouch();
        void onChange(FloowerChangeCallback callback);

        void setPetalsOpenLevel(int8_t level, int transitionTime = 0);
        int8_t getPetalsOpenLevel();
        int8_t getCurrentPetalsOpenLevel();
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

        void showStatus(HsbColor color, FloowerStatusAnimation animation, int duration);

        PowerState readPowerState();
        bool isUsbPowered();

    private:
        bool setStepperPowerOn(bool powerOn);
        bool setPixelsPowerOn(bool powerOn);

        NeoPixelAnimator animations; // animation management object used for both servo and pixels to animate
        void pixelsTransitionAnimationUpdate(const AnimationParam& param);
        void pixelsFlashAnimationUpdate(const AnimationParam& param);
        void pixelsRainbowAnimationUpdate(const AnimationParam& param);
        void pixelsRainbowLoopAnimationUpdate(const AnimationParam& param);
        void pixelsCandleAnimationUpdate(const AnimationParam& param);
        void showColor(HsbColor color);
        void statusBlinkOnceAnimationUpdate(const AnimationParam& param);
        void statusPulsatingAnimationUpdate(const AnimationParam& param);

        static void touchISR();

        Config *config;
        FloowerChangeCallback changeCallback;

        // petals motor
        Petals *petals;

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

        // status LED
        HsbColor statusColor = colorBlack;
        NeoPixelBus<NeoGrbFeature, NeoEsp32I2s1800KbpsMethod> statusPixel;

        // touch
        FloowerOnLeafTouchCallback touchCallback;
        static unsigned long touchStartedTime;
        static unsigned long touchEndedTime;
        static unsigned long lastTouchTime;
        bool touchRegistered = false;
        bool holdTouchRegistered = false;
        bool longTouchRegistered = false;

        // battery
        PowerState powerState;
};