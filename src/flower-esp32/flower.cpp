#include "flower.h"

#define NEOPIXEL_PIN 27
#define NEOPIXEL_PWR_PIN 25

#define SERVO_PIN 26
#define SERVO_PWR_PIN 33
#define SERVO_POWER_OFF_DELAY 500

#define TOUCH_SENSOR_PIN 4
#define TOUCH_TRESHOLD 45 // 45
#define TOUCH_TIMEOUT 500 // 

#define BATTERY_ANALOG_IN 36 // VP

#define ACTY_LED_PIN 2
#define ACTY_BLINK_TIME 50

Flower::Flower() : animations(2), pixels(7, NEOPIXEL_PIN) {
}

void Flower::init(byte ledsModel) {
  // LEDs
  pixelsPowerOn = true; // to make setPixelsPowerOn effective
  setPixelsPowerOn(false);
  pinMode(NEOPIXEL_PWR_PIN, OUTPUT);
  pixels.init(ledsModel);

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

void Flower::initServo(int closedAngle, int openAngle) {
  // default servo configuration
  servoOpenAngle = openAngle;
  servoClosedAngle = closedAngle;
  servoAngle = closedAngle;
  servoOriginAngle = closedAngle;
  servoTargetAngle = closedAngle;
  petalsOpenLevel = -1; // 0-100% (-1 unknown)

  // servo
  servoPowerOn = true; // to make setServoPowerOn effective
  setServoPowerOn(false);
  pinMode(SERVO_PWR_PIN, OUTPUT);

  servo.setPeriodHertz(50); // standard 50 Hz servo
  servo.attach(SERVO_PIN, servoClosedAngle, servoOpenAngle);
  servo.write(servoAngle);
}

void Flower::update() {
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
}

void Flower::setPetalsOpenLevel(byte level, int transitionTime) {
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

  Serial.print("Petals level ");
  Serial.print(petalsOpenLevel);
  Serial.print("% (");
  Serial.print(newAngle);
  Serial.println(")");

  // TODO: support transitionTime of 0
  animations.StartAnimation(0, transitionTime, [=](const AnimationParam& param){ servoAnimationUpdate(param); });
}

void Flower::servoAnimationUpdate(const AnimationParam& param) {
  servoAngle = servoOriginAngle + (servoTargetAngle - servoOriginAngle) * param.progress;

  setServoPowerOn(true);
  servo.write(servoAngle);

  if (param.state == AnimationState_Completed) {
    servoPowerOffTime = millis() + SERVO_POWER_OFF_DELAY;
  }
}

void Flower::setColor(RgbColor color, FlowerColorMode colorMode, int transitionTime) {
  if (color.R == pixelsTargetColor.R && color.G == pixelsTargetColor.G && color.B == pixelsTargetColor.B && pixelsColorMode == colorMode) {
    return; // no change
  }

  pixelsColorMode = colorMode;
  pixelsTargetColor = color;

  Serial.print("Flower color ");
  Serial.print(color.R);
  Serial.print(",");
  Serial.print(color.G);
  Serial.print(",");
  Serial.println(color.B);

  if (colorMode == TRANSITION) {
    pixelsOriginColor = pixelsColor;
    animations.StartAnimation(1, transitionTime, [=](const AnimationParam& param){ pixelsTransitionAnimationUpdate(param); });  
  }
  if (colorMode == PULSE) {
    pixelsOriginColor = colorBlack; //RgbColor::LinearBlend(color, colorBlack, 0.95);
    animations.StartAnimation(1, transitionTime, [=](const AnimationParam& param){ pixelsPulseAnimationUpdate(param); });
  }
}

void Flower::pixelsTransitionAnimationUpdate(const AnimationParam& param) {
  float progress = NeoEase::CubicOut(param.progress);
  pixelsColor = RgbColor::LinearBlend(pixelsOriginColor, pixelsTargetColor, progress);
  showColor(pixelsColor);
}

void Flower::pixelsPulseAnimationUpdate(const AnimationParam& param) {
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

void Flower::showColor(RgbColor color) {
  if (!lowPowerMode) {
    pixels.ClearTo(color);
  }
  else {
    pixels.ClearTo(colorBlack);
    pixels.SetPixelColor(0, color);
  }
}

boolean Flower::isAnimating() {
  return animations.IsAnimating();
}

boolean Flower::isIdle() {
  return !isAnimating();
}

void Flower::onLeafTouch(void (*callback)()) {
  touchAttachInterrupt(TOUCH_SENSOR_PIN, callback, TOUCH_TRESHOLD);
  touchCallback = callback;
}

void Flower::acty() {
  digitalWrite(ACTY_LED_PIN, HIGH);
  actyOffTime = millis() + ACTY_BLINK_TIME;
}

boolean Flower::setPixelsPowerOn(boolean powerOn) {
  if (powerOn && !pixelsPowerOn) {
    pixelsPowerOn = true;
    Serial.println("LEDs power ON");
    digitalWrite(NEOPIXEL_PWR_PIN, LOW);
    delay(5); // TODO
    return true;
  }
  if (!powerOn && pixelsPowerOn) {
    pixelsPowerOn = false;
    Serial.println("LEDs power OFF");
    digitalWrite(NEOPIXEL_PWR_PIN, HIGH);
    return true;
  }
  return false; // no change
}

boolean Flower::setServoPowerOn(boolean powerOn) {
  if (powerOn && !servoPowerOn) {
    servoPowerOffTime = 0;
    servoPowerOn = true;
    Serial.println("Servo power ON");
    digitalWrite(SERVO_PWR_PIN, LOW);
    delay(5); // TODO
    return true;
  }
  if (!powerOn && servoPowerOn) {
    servoPowerOffTime = 0;
    servoPowerOn = false;
    Serial.println("Servo power OFF");
    digitalWrite(SERVO_PWR_PIN, HIGH);
    return true;
  }
  return false; // no change
}

float Flower::readBatteryVoltage() {
  float reading = analogRead(BATTERY_ANALOG_IN); // 0-4095
  float voltage = reading * 0.00181; // 1/4069 for scale * analog reference voltage is 3.6V * 2 for using 1:1 voltage divider + adjustment

  byte level = _min(_max(0, voltage - 3.3) * 125, 100); // 3.3 .. 0%, 4.1 .. 100%

  Serial.print("Battery ");
  Serial.print(reading);
  Serial.print(" ");
  Serial.print(voltage);
  Serial.print("V ");
  Serial.print(level);
  Serial.println("%");

  batteryVoltage = voltage;
  return voltage;
}

void Flower::setLowPowerMode(boolean lowPowerMode) {
  if (this->lowPowerMode != lowPowerMode) {
    this->lowPowerMode = lowPowerMode;
    showColor(pixelsColor);
  }
}

bool Flower::isLowPowerMode() {
  return lowPowerMode;
}

void Flower::handleTimers() {
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
