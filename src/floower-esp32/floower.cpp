#include "floower.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "Floower";
#endif

#define NEOPIXEL_PIN 27
#define NEOPIXEL_PWR_PIN 25

#define SERVO_PIN 26
#define SERVO_PWR_PIN 33
#define SERVO_POWER_OFF_DELAY 500

#define TOUCH_SENSOR_PIN 4
#define TOUCH_FADE_TIME 50 // 50
#define TOUCH_HOLD_TIME_TRESHOLD 2000 // 2s to recognize hold touch
#define TOUCH_COOlDOWN_TIME 300 // prevent random touch within 300ms after last touch

#define BATTERY_ANALOG_PIN 36 // VP
#define USB_ANALOG_PIN 39 // VN
//#define CHARGE_PIN 15

#define ACTY_LED_PIN 2
#define ACTY_BLINK_TIME 50

Floower::Floower(Config *config) : config(config), animations(2), pixels(7, NEOPIXEL_PIN) {}

void Floower::init() {
  // LEDs
  pixelsPowerOn = true; // to make setPixelsPowerOn effective
  setPixelsPowerOn(false);
  pinMode(NEOPIXEL_PWR_PIN, OUTPUT);

  pixelsColor = colorBlack;
  RgbColor pixelsOriginColor = colorBlack;
  RgbColor pixelsTargetColor = colorBlack;
  pixels.Begin();

  // configure ADC for battery level reading
  analogReadResolution(12); // se0t 12bit resolution (0-4095)
  analogSetAttenuation(ADC_11db); // set AREF to be 3.6V
  analogSetCycles(8); // num of cycles per sample, 8 is default optimal
  analogSetSamples(1); // num of samples

  // acty LED
  pinMode(ACTY_LED_PIN, OUTPUT);
  digitalWrite(ACTY_LED_PIN, HIGH);
}

void Floower::initServo() {
  // default servo configuration
  servoOpenAngle = config->servoOpen;
  servoClosedAngle = config->servoClosed;
  servoAngle = config->servoClosed;
  servoOriginAngle = config->servoClosed;
  servoTargetAngle = config->servoClosed;
  petalsOpenLevel = -1; // 0-100% (-1 unknown)

  // servo
  servoPowerOn = true; // to make setServoPowerOn effective
  setServoPowerOn(false);
  pinMode(SERVO_PWR_PIN, OUTPUT);

  servo.setPeriodHertz(50); // standard 50 Hz servo
  servo.attach(SERVO_PIN, servoClosedAngle, servoOpenAngle);
  servo.write(servoAngle);
}

void Floower::update() {
  animations.UpdateAnimations();
  handleTimers();

  // show pixels
  if (pixelsColor.CalculateBrightness() > 0) {
    setPixelsPowerOn(true);
    pixels.Show();
  }
  else {
    setPixelsPowerOn(false);
  }

  if (touchStartedTime > 0) {
    unsigned long now = millis();
    unsigned int touchTime = now - touchStartedTime;
    unsigned long sinceLastTouch = millis() - lastTouchTime;

    if (!touchRegistered) {
      ESP_LOGD(LOG_TAG, "Touch");
      touchRegistered = true;
      touchCallback(FloowerTouchType::TOUCH);
    }
    if (!holdTouchRegistered && touchTime > TOUCH_HOLD_TIME_TRESHOLD) {
      ESP_LOGD(LOG_TAG, "Hold Touch %d", touchTime);
      holdTouchRegistered = true;
      touchCallback(FloowerTouchType::HOLD);
    }
    if (sinceLastTouch > TOUCH_FADE_TIME) {
      ESP_LOGD(LOG_TAG, "Touch release");
      touchStartedTime = 0;
      touchEndedTime = now;
      touchRegistered = false;
      holdTouchRegistered = false;
      touchCallback(FloowerTouchType::RELEASE);
    }
  }
  else if (touchEndedTime > 0 && millis() - touchEndedTime > TOUCH_COOlDOWN_TIME) {
    ESP_LOGD(LOG_TAG, "Touch enabled");
    touchEndedTime = 0;
  }
}

void Floower::touchISR() {
  lastTouchTime = millis();
  if (touchStartedTime == 0 && touchEndedTime == 0) {
    touchStartedTime = lastTouchTime;
  }
}

void Floower::touchAttachInterruptProxy(void (*callback)()) {
  touchAttachInterrupt(TOUCH_SENSOR_PIN, callback, 45);
}

void Floower::onLeafTouch(void (*callback)(FloowerTouchType type)) {
  touchCallback = callback;
}

void Floower::setPetalsOpenLevel(byte level, int transitionTime) {
  if (level == petalsOpenLevel) {
    return; // no change, keep doing the old movement until done
  }
  petalsOpenLevel = level;

  int newAngle;
  if (level >= 100) {
    newAngle = servoOpenAngle;
  }
  else {
    float position = (servoOpenAngle - servoClosedAngle);
    position = position * level / 100.0;
    newAngle = servoClosedAngle + position;
  }

  servoOriginAngle = servoAngle;
  servoTargetAngle = newAngle;

  ESP_LOGI(LOG_TAG, "Petals %d%% (%d)", petalsOpenLevel, newAngle);

  // TODO: support transitionTime of 0
  animations.StartAnimation(0, transitionTime, [=](const AnimationParam& param){ servoAnimationUpdate(param); });
}

void Floower::servoAnimationUpdate(const AnimationParam& param) {
  servoAngle = servoOriginAngle + (servoTargetAngle - servoOriginAngle) * param.progress;

  setServoPowerOn(true);
  servo.write(servoAngle);

  if (param.state == AnimationState_Completed) {
    servoPowerOffTime = millis() + SERVO_POWER_OFF_DELAY;
  }
}

byte Floower::getPetalOpenLevel() {
  return servoAngle;
}

void Floower::setColor(RgbColor color, FloowerColorMode colorMode, int transitionTime) {
  if (color.R == pixelsTargetColor.R && color.G == pixelsTargetColor.G && color.B == pixelsTargetColor.B && pixelsColorMode == colorMode) {
    return; // no change
  }

  pixelsColorMode = colorMode;
  pixelsTargetColor = color;

  ESP_LOGI(LOG_TAG, "Color %d,%d,%d", color.R, color.G, color.B);

  if (colorMode == TRANSITION) {
    pixelsOriginColor = pixelsColor;
    animations.StartAnimation(1, transitionTime, [=](const AnimationParam& param){ pixelsTransitionAnimationUpdate(param); });  
  }
  if (colorMode == PULSE) {
    pixelsOriginColor = colorBlack; //RgbColor::LinearBlend(color, colorBlack, 0.95);
    animations.StartAnimation(1, transitionTime, [=](const AnimationParam& param){ pixelsPulseAnimationUpdate(param); });
  }
}

void Floower::pixelsTransitionAnimationUpdate(const AnimationParam& param) {
  pixelsColor = RgbColor::LinearBlend(pixelsOriginColor, pixelsTargetColor, param.progress);
  showColor(pixelsColor);
}

void Floower::pixelsPulseAnimationUpdate(const AnimationParam& param) {
  if (param.progress < 0.5) {
    float progress = NeoEase::CubicInOut(param.progress * 2);
    pixelsColor = RgbColor::LinearBlend(pixelsOriginColor, pixelsTargetColor, progress);
  }
  else {
    float progress = NeoEase::CubicInOut((1 - param.progress) * 2);
    pixelsColor = RgbColor::LinearBlend(pixelsOriginColor, pixelsTargetColor, progress);
  }

  showColor(pixelsColor);

  if (param.state == AnimationState_Completed) {
    if (pixelsTargetColor.CalculateBrightness() > 0) { // while there is something to show
      animations.RestartAnimation(param.index);
    }
  }
}

RgbColor Floower::getColor() {
  return pixelsColor;
}

void Floower::startColorPicker() {
  pixelsOriginColor = pixelsColor;
  animations.StartAnimation(1, 10000, [=](const AnimationParam& param){ colorPickerAnimationUpdate(param); });
}

void Floower::stopColorPicker() {
  animations.StopAnimation(1);
}

void Floower::colorPickerAnimationUpdate(const AnimationParam& param) {
  HsbColor hsbOriginal = HsbColor(pixelsOriginColor);
  float hue = hsbOriginal.H + param.progress;
  if (hue > 1.0) {
    hue = hue - 1;
  }
  pixelsColor = RgbColor(HsbColor(hue, 1, 0.4)); // TODO: fine tune
  showColor(pixelsColor);

  if (param.state == AnimationState_Completed) {
    if (pixelsTargetColor.CalculateBrightness() > 0) { // while there is something to show
      animations.RestartAnimation(param.index);
    }
  }
}

void Floower::showColor(RgbColor color) {
  if (!lowPowerMode) {
    pixels.ClearTo(color);
  }
  else {
    pixels.ClearTo(colorBlack);
    pixels.SetPixelColor(0, color);
  }
}

boolean Floower::arePetalsMoving() {
  return animations.IsAnimationActive(0);
}

boolean Floower::isIdle() {
  return !animations.IsAnimating();
}

void Floower::acty() {
  digitalWrite(ACTY_LED_PIN, HIGH);
  actyOffTime = millis() + ACTY_BLINK_TIME;
}

boolean Floower::setPixelsPowerOn(boolean powerOn) {
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

boolean Floower::setServoPowerOn(boolean powerOn) {
  if (powerOn && !servoPowerOn) {
    servoPowerOffTime = 0;
    servoPowerOn = true;
    ESP_LOGD(LOG_TAG, "Servo power ON");
    digitalWrite(SERVO_PWR_PIN, LOW);
    delay(5); // TODO
    return true;
  }
  if (!powerOn && servoPowerOn) {
    servoPowerOffTime = 0;
    servoPowerOn = false;
    ESP_LOGD(LOG_TAG, "Servo power OFF");
    digitalWrite(SERVO_PWR_PIN, HIGH);
    return true;
  }
  return false; // no change
}

Battery Floower::readBatteryState() {
  float reading = analogRead(BATTERY_ANALOG_PIN); // 0-4095
  float voltage = reading * 0.00181; // 1/4069 for scale * analog reference voltage is 3.6V * 2 for using 1:1 voltage divider + adjustment

  byte level = _min(_max(0, voltage - 3.3) * 125, 100); // 3.3 .. 0%, 4.1 .. 100%

  ESP_LOGI(LOG_TAG, "Battery %.0f %.2fV %d%%", reading, voltage, level);

  batteryState = {voltage, level};
  return batteryState;
}

bool Floower::isUSBPowered() {
  float reading = analogRead(USB_ANALOG_PIN); // 0-4095
  return reading > 2000; // ~2900 is 5V
}

void Floower::setLowPowerMode(boolean lowPowerMode) {
  if (this->lowPowerMode != lowPowerMode) {
    this->lowPowerMode = lowPowerMode;
    showColor(pixelsColor);
  }
}

bool Floower::isLowPowerMode() {
  return lowPowerMode;
}

void Floower::handleTimers() {
  long now = millis();

  if (servoPowerOffTime > 0 && servoPowerOffTime < now) {
    servoPowerOffTime = 0;
    setServoPowerOn(false);
  }
  if (actyOffTime > 0 && actyOffTime < now) {
    actyOffTime = 0;
    digitalWrite(ACTY_LED_PIN, LOW); 
  }
}
