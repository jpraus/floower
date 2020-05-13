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

void Flower::init(int closedAngle, int openAngle, byte ledsModel) {
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

void Flower::setColor(RgbColor color, int transitionTime) {
  if (color.R == pixelsColor.R && color.G == pixelsColor.G && color.B == pixelsColor.B) {
    return; // no change
  }
  pixelsOriginColor = pixelsColor;
  pixelsTargetColor = color;

  Serial.print("Flower color ");
  Serial.print(color.R);
  Serial.print(",");
  Serial.print(color.G);
  Serial.print(",");
  Serial.println(color.B);

  animations.StartAnimation(1, transitionTime, [=](const AnimationParam& param){ pixelsAnimationUpdate(param); });
}

void Flower::pixelsAnimationUpdate(const AnimationParam& param) {
  pixelsColor = RgbColor::LinearBlend(pixelsOriginColor, pixelsTargetColor, param.progress);
  pixels.ClearTo(pixelsColor);
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
  float voltage = reading / 4096.0 * 3.7 * 2; // Analog reference voltage is 3.6V, using 1:1 voltage divider (*2)

  Serial.print("Battery ");
  Serial.print(reading);
  Serial.print(" ");
  Serial.print(voltage);
  Serial.println("V");

  return voltage;
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
