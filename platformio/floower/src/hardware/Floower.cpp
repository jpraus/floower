#include "Floower.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "Floower";
#endif

#define NEOPIXEL_PIN 27 // 27
#define NEOPIXEL_PWR_PIN 25

#define BATTERY_ANALOG_PIN 36 // VP
#define USB_ANALOG_PIN 39 // VN
#define CHARGE_PIN 35

#define STATUS_NEOPIXEL_PIN 32
#define ACTY_LED_PIN 2
#define ACTY_BLINK_TIME 50

#define ANIMATIONS_INDECES 3
#define ANIMATION_INDEX_LEDS 1
#define ANIMATION_INDEX_STATUS 2

#define TOUCH_SENSOR_PIN 4
#define TOUCH_FADE_TIME 75 // 50
#define TOUCH_LONG_TIME_THRESHOLD 2000 // 2s to recognize long touch
#define TOUCH_HOLD_TIME_THRESHOLD 5000 // 5s to recognize hold touch
#define TOUCH_COOLDOWN_TIME 300 // prevent random touch within 300ms after last touch

unsigned long Floower::touchStartedTime = 0;
unsigned long Floower::touchEndedTime = 0;
unsigned long Floower::lastTouchTime = 0;

const HsbColor candleColor(0.042, 1.0, 1.0); // candle orange color

Floower::Floower(Config *config) 
        : animations(ANIMATIONS_INDECES), config(config), pixels(7, NEOPIXEL_PIN), statusPixel(2, STATUS_NEOPIXEL_PIN) {
    pinMode(CHARGE_PIN, INPUT);
}

void Floower::init() {
    // petals instance
    if (config->hardwareRevision >= 9) {
        ESP_LOGI(LOG_TAG, "Using STEPPER");
        petals = new StepperPetals(config);
    }
    else {
        ESP_LOGI(LOG_TAG, "Using SERVO");
        petals = new ServoPetals(config);
    }

    // LEDs
    pixelsPowerOn = true; // to make setPixelsPowerOn effective
    setPixelsPowerOn(false);
    pinMode(NEOPIXEL_PWR_PIN, OUTPUT);

    pixelsColor = colorBlack;
    pixelsOriginColor = colorBlack;
    pixelsTargetColor = colorBlack;
    pixels.Begin();
    statusPixel.ClearTo(pixelsColor);
    pixels.Show();

    statusColor = colorBlack;
    statusPixel.Begin();
    statusPixel.ClearTo(statusColor);
    statusPixel.Show();

    // configure ADC for battery level reading
    analogReadResolution(12); // se0t 12bit resolution (0-4095)
    analogSetAttenuation(ADC_11db); // set AREF to be 3.6V
    //analogSetCycles(8); // num of cycles per sample, 8 is default optimal
    //analogSetSamples(1); // num of samples
}

void Floower::initPetals(long currentPosition) {
    petals->init(currentPosition);
}

void Floower::update() {
    petals->update();
    animations.UpdateAnimations();

    // show pixels
    if (pixelsColor.B > 0) {
        setPixelsPowerOn(true);
        if (pixels.IsDirty() && pixels.CanShow()) {
            pixels.Show();
        }
    }
    else if (pixelsPowerOn) {
        pixels.Show();
        setPixelsPowerOn(false);
    }
    if (statusPixel.IsDirty() && statusPixel.CanShow()) {
        statusPixel.Show();
    }

    unsigned long now = millis();
    if (touchStartedTime > 0) {
        unsigned int touchTime = now - touchStartedTime;
        unsigned long sinceLastTouch = now - lastTouchTime;

        if (!touchRegistered) {
            ESP_LOGI(LOG_TAG, "Touch Down");
            touchRegistered = true;
            if (touchCallback != nullptr) {
                touchCallback(FloowerTouchEvent::TOUCH_DOWN);
            }
        }
        if (!longTouchRegistered && touchTime > TOUCH_LONG_TIME_THRESHOLD) {
            ESP_LOGI(LOG_TAG, "Long Touch %d", touchTime);
            longTouchRegistered = true;
            if (touchCallback != nullptr) {
                touchCallback(FloowerTouchEvent::TOUCH_LONG);
            }
        }
        if (!holdTouchRegistered && touchTime > TOUCH_HOLD_TIME_THRESHOLD) {
            ESP_LOGI(LOG_TAG, "Hold Touch %d", touchTime);
            holdTouchRegistered = true;
            if (touchCallback != nullptr) {
                touchCallback(FloowerTouchEvent::TOUCH_HOLD);
            }
        }
        if (sinceLastTouch > TOUCH_FADE_TIME) {
            ESP_LOGI(LOG_TAG, "Touch Up %d", sinceLastTouch);
            touchStartedTime = 0;
            touchEndedTime = now;
            touchRegistered = false;
            longTouchRegistered = false;
            holdTouchRegistered = false;
            if (touchCallback != nullptr) {
                touchCallback(FloowerTouchEvent::TOUCH_UP);
            }
        }
    }
    else if (touchEndedTime > 0 && now - touchEndedTime > TOUCH_COOLDOWN_TIME) {
        ESP_LOGI(LOG_TAG, "Touch enabled");
        touchEndedTime = 0;
    }
}

void Floower::registerOutsideTouch() {
    touchISR();
}

void Floower::enableTouch(FloowerOnLeafTouchCallback callback, bool defer) {
    touchCallback = callback;
    touchAttachInterrupt(TOUCH_SENSOR_PIN, Floower::touchISR, config->touchThreshold);
    if (defer) {
        touchEndedTime = millis();
    }
    else {
        ESP_LOGI(LOG_TAG, "Touch enabled");
    }
}

void Floower::reconfigureTouch() {
    detachInterrupt(TOUCH_SENSOR_PIN);
    touchAttachInterrupt(TOUCH_SENSOR_PIN, Floower::touchISR, config->touchThreshold);
    ESP_LOGI(LOG_TAG, "Touch reconfigured");
}

void Floower::disableTouch() {
    detachInterrupt(TOUCH_SENSOR_PIN);
    ESP_LOGI(LOG_TAG, "Touch disabled");
}

void Floower::touchISR() {
    lastTouchTime = millis();
    if (touchStartedTime == 0 && touchEndedTime == 0) {
        touchStartedTime = lastTouchTime;
    }
}

uint8_t Floower::readTouch() {
    return touchRead(TOUCH_SENSOR_PIN);
}

void Floower::onChange(FloowerChangeCallback callback) {
    changeCallback = callback;
}

void Floower::setPetalsOpenLevel(int8_t level, int transitionTime) {
    petals->setPetalsOpenLevel(level, transitionTime);

    if (changeCallback != nullptr) {
        changeCallback(level, pixelsTargetColor);
    }
}

int8_t Floower::getPetalsOpenLevel() {
    return petals->getPetalsOpenLevel();
}

int8_t Floower::getCurrentPetalsOpenLevel() {
    return petals->getCurrentPetalsOpenLevel();
}

bool Floower::arePetalsMoving() {
    return petals->arePetalsMoving();
}

void Floower::transitionColorBrightness(double brightness, int transitionTime) {
    if (brightness == pixelsTargetColor.B) {
        return; // no change
    }
    transitionColor(pixelsTargetColor.H, pixelsTargetColor.S, brightness, transitionTime);
}

void Floower::transitionColor(double hue, double saturation, double brightness, int transitionTime) {
    if (hue == pixelsTargetColor.H && saturation == pixelsTargetColor.S && brightness == pixelsTargetColor.B) {
        return; // no change
    }

    // make smooth transition
    if (pixelsColor.B == 0) { // current color is black
        pixelsColor.H = hue;
        pixelsColor.S = saturation;
    }
    else if (brightness == 0) { // target color is black
        hue = pixelsColor.H;
        saturation = pixelsColor.S;
    }

    pixelsTargetColor = HsbColor(hue, saturation, brightness);
    interruptiblePixelsAnimation = false;

    ESP_LOGI(LOG_TAG, "Color %.2f,%.2f,%.2f", pixelsTargetColor.H, pixelsTargetColor.S, pixelsTargetColor.B);

    if (transitionTime <= 0) {
        pixelsColor = pixelsTargetColor;
        showColor(pixelsColor);
    }
    else {
        pixelsOriginColor = pixelsColor;
        animations.StartAnimation(ANIMATION_INDEX_LEDS, transitionTime, [=](const AnimationParam& param){ pixelsTransitionAnimationUpdate(param); });  
    }

    if (changeCallback != nullptr) {
        changeCallback(getPetalsOpenLevel(), pixelsTargetColor);
    }
}

void Floower::pixelsTransitionAnimationUpdate(const AnimationParam& param) {
    double diff = pixelsOriginColor.H - pixelsTargetColor.H;
    if (diff < 0.2 && diff > -0.2) {
        pixelsColor = HsbColor::LinearBlend<NeoHueBlendShortestDistance>(pixelsOriginColor, pixelsTargetColor, param.progress);
    }
    else {
        pixelsColor = RgbColor::LinearBlend(pixelsOriginColor, pixelsTargetColor, param.progress);
    }
    showColor(pixelsColor);
}

void Floower::flashColor(double hue, double saturation, int flashDuration) {
    pixelsTargetColor = HsbColor(hue, saturation, 1.0);
    pixelsColor = pixelsTargetColor;
    pixelsColor.B = 0;

    interruptiblePixelsAnimation = false;
    animations.StartAnimation(ANIMATION_INDEX_LEDS, flashDuration, [=](const AnimationParam& param){ pixelsFlashAnimationUpdate(param); });
}

void Floower::pixelsFlashAnimationUpdate(const AnimationParam& param) {
    if (param.progress < 0.5) {
        pixelsColor.B = NeoEase::CubicInOut(param.progress * 2);
    }
    else {
        pixelsColor.B = NeoEase::CubicInOut((1 - param.progress) * 2);
    }

    showColor(pixelsColor);

    if (param.state == AnimationState_Completed) {
        if (pixelsTargetColor.B > 0) { // while there is something to show
            animations.RestartAnimation(param.index);
        }
    }
}

HsbColor Floower::getColor() {
    return pixelsTargetColor;
}

HsbColor Floower::getCurrentColor() {
    return pixelsColor;
}

void Floower::startAnimation(FloowerColorAnimation animation) {
    interruptiblePixelsAnimation = true;

    if (animation == RAINBOW) {
        pixelsOriginColor = pixelsColor;
        animations.StartAnimation(ANIMATION_INDEX_LEDS, 10000, [=](const AnimationParam& param){ pixelsRainbowAnimationUpdate(param); });
    }
    else if (animation == RAINBOW_LOOP) {
        pixelsTargetColor = pixelsColor = colorWhite;
        animations.StartAnimation(ANIMATION_INDEX_LEDS, 10000, [=](const AnimationParam& param){ pixelsRainbowLoopAnimationUpdate(param); });
    }
    else if (animation == CANDLE) {
        pixelsTargetColor = pixelsColor = candleColor; // candle orange
        for (uint8_t i = 0; i < 6; i++) {
            candleOriginColors[i] = pixelsTargetColor;
            candleTargetColors[i] = pixelsTargetColor;
        }
        animations.StartAnimation(ANIMATION_INDEX_LEDS, 100, [=](const AnimationParam& param){ pixelsCandleAnimationUpdate(param); });
    }
}

void Floower::stopAnimation(bool retainColor) {
    animations.StopAnimation(ANIMATION_INDEX_LEDS);
    if (retainColor) {
        pixelsTargetColor = pixelsColor;
    }
}

void Floower::pixelsRainbowAnimationUpdate(const AnimationParam& param) {
    float hue = pixelsOriginColor.H + param.progress;
    if (hue >= 1.0) {
        hue = hue - 1;
    }
    pixelsColor = HsbColor(hue, 1, pixelsOriginColor.B);
    showColor(pixelsColor);

    if (param.state == AnimationState_Completed) {
        if (pixelsTargetColor.B > 0) { // while there is something to show
            animations.RestartAnimation(param.index);
        }
    }
}

void Floower::pixelsRainbowLoopAnimationUpdate(const AnimationParam& param) {
    double hue = param.progress;
    double step = 1.0 / 6.0;
    pixels.SetPixelColor(0, HsbColor(param.progress, 1, config->colorBrightness));
    for (uint8_t i = 1; i < 7; i++, hue += step) {
        if (hue >= 1.0) {
            hue = hue - 1;
        }
        pixels.SetPixelColor(i, HsbColor(hue, 1, config->colorBrightness));
    }
    if (param.state == AnimationState_Completed) {
        animations.RestartAnimation(param.index);
    }
}

void Floower::pixelsCandleAnimationUpdate(const AnimationParam& param) {
    pixels.SetPixelColor(0, pixelsTargetColor);
    for (uint8_t i = 0; i < 6; i++) {
        pixels.SetPixelColor(i + 1, HsbColor::LinearBlend<NeoHueBlendShortestDistance>(candleOriginColors[i], candleTargetColors[i], param.progress));
    }

    if (param.state == AnimationState_Completed) {
        for (uint8_t i = 0; i < 6; i++) {
            candleOriginColors[i] = candleTargetColors[i];
            candleTargetColors[i] = HsbColor(candleColor.H, candleColor.S, random(20, 100) / 100.0);
        }
        animations.StartAnimation(param.index, random(10, 400), [=](const AnimationParam& param){ pixelsCandleAnimationUpdate(param); });
    }
}

void Floower::showColor(HsbColor color) {
    pixels.ClearTo(color);
}

bool Floower::isLit() {
    return pixelsPowerOn;
}

bool Floower::isAnimating() {
    return animations.IsAnimationActive(ANIMATION_INDEX_LEDS) || petals->arePetalsMoving();
}

bool Floower::isChangingColor() {
    return (!interruptiblePixelsAnimation && animations.IsAnimationActive(ANIMATION_INDEX_LEDS));
}

void Floower::showStatus(HsbColor color, FloowerStatusAnimation animation, int duration) {
    statusColor = color;
    statusPixel.SetPixelColor(0, color);

    if (animation == STILL) {
        animations.StopAnimation(ANIMATION_INDEX_STATUS);    
    }
    else if (animation == BLINK_ONCE) {
        animations.StartAnimation(ANIMATION_INDEX_STATUS, duration, [=](const AnimationParam& param){ statusBlinkOnceAnimationUpdate(param); });
    }
    else if (animation == PULSATING) {
        animations.StartAnimation(ANIMATION_INDEX_STATUS, duration, [=](const AnimationParam& param){ statusPulsatingAnimationUpdate(param); });
    }
}

void Floower::statusBlinkOnceAnimationUpdate(const AnimationParam& param) {
    if (param.state == AnimationState_Completed) {
        statusColor = colorBlack;
        statusPixel.SetPixelColor(0, statusColor);
    }
}

void Floower::statusPulsatingAnimationUpdate(const AnimationParam& param) {
    if (param.progress < 0.5) {
        statusColor.B = NeoEase::CubicInOut(0.75 - param.progress);
    }
    else {
        statusColor.B = NeoEase::CubicInOut(param.progress - 0.25);
    }

    statusPixel.SetPixelColor(0, statusColor);

    if (param.state == AnimationState_Completed) {
        if (statusColor.B > 0) { // while there is something to show
        animations.RestartAnimation(param.index);
        }
    }
}

bool Floower::setPixelsPowerOn(bool powerOn) {
    if (powerOn && !pixelsPowerOn) {
        pixelsPowerOn = true;
        ESP_LOGD(LOG_TAG, "LEDs power ON");
        digitalWrite(NEOPIXEL_PWR_PIN, LOW);
        delay(5); // TODO
        return true;
    }
    if (!powerOn && pixelsPowerOn) {
        pixelsPowerOn = false;
        ESP_LOGD(LOG_TAG, "LEDs power OFF");
        digitalWrite(NEOPIXEL_PWR_PIN, HIGH);
        return true;
    }
    return false; // no change
}

PowerState Floower::readPowerState() {
    float reading = analogRead(BATTERY_ANALOG_PIN); // 0-4095
    float voltage = reading * 0.00181; // 1/4069 for scale * analog reference voltage is 3.6V * 2 for using 1:1 voltage divider + adjustment
    uint8_t level = _min(_max(0, voltage - 3.3) * 111, 100); // 3.3 .. 0%, 4.2 .. 100%

    bool charging = digitalRead(CHARGE_PIN) == LOW;
    bool switchedOn = reading > 0; // there is voltage of battery present
    bool usbPowered = true;

    if (config->hardwareRevision > 5) { // logic board with revision 5 lack the USB detection circuitry, pretend its always charging
        usbPowered = analogRead(USB_ANALOG_PIN) > 2000; // ~2900 is 5V
    }

    ESP_LOGI(LOG_TAG, "Battery %.0f %.2fV %d%% %s", reading, voltage, level, charging ? "CHRG" : (usbPowered ? "USB" : ""));

    powerState = {voltage, level, charging, usbPowered, switchedOn};
    return powerState;
}

bool Floower::isUsbPowered() {
    return powerState.usbPowered;
}