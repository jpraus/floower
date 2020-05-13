#include "flower.h"

#define NEOPIXEL_PIN 27
#define NEOPIXEL_PWR_PIN 25

#define SERVO_PIN 26
#define SERVO_PWR_PIN 33

Flower::Flower() : animations(2) {
}

void Flower::init(int closedAngle, int openAngle) {
  // default servo configuration
  servoOpenAngle = openAngle;
  servoClosedAngle = closedAngle;
  servoAngle = closedAngle;
  servoTargetAngle = closedAngle;
  petalsOpenLevel = 0; // 0-100%

  servoPowerOn = false;
  setServoPowerOn(false);
  pinMode(SERVO_PWR_PIN, OUTPUT);

  servo.setPeriodHertz(50); // standard 50 Hz servo
  servo.attach(SERVO_PIN, servoClosedAngle, servoOpenAngle);
  servo.write(servoAngle);

  setPixelsPowerOn(false);
  pinMode(NEOPIXEL_PWR_PIN, OUTPUT);
}

void Flower::update() {
  animations.UpdateAnimations();
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

  servoTargetAngle = newAngle;

  Serial.print("Petals level ");
  Serial.print(petalsOpenLevel);
  Serial.print("% (");
  Serial.print(newAngle);
  Serial.println(")");

  animations.StartAnimation(1, transitionTime, [=](const AnimationParam& param){ servoAnimationUpdate(param); });
}

void Flower::servoAnimationUpdate(const AnimationParam& param) {
  if (param.state == AnimationState_Completed) {
    servoAngle = servoTargetAngle;
    setServoPowerOn(false);
  }
  else {
    int newAngle = servoAngle + (servoTargetAngle - servoAngle) * param.progress;
    //Serial.print("angle ");
    //Serial.println(newAngle);

    setServoPowerOn(true);
    servo.write(newAngle);
  }
}

bool Flower::setPixelsPowerOn(boolean powerOn) {
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

bool Flower::setServoPowerOn(boolean powerOn) {
  if (powerOn && !servoPowerOn) {
    servoPowerOn = true;
    Serial.println("Servo power ON");
    digitalWrite(SERVO_PWR_PIN, LOW);
    delay(5); // TODO
    return true;
  }
  if (!powerOn && servoPowerOn) {
    servoPowerOn = false;
    Serial.println("Servo power OFF");
    digitalWrite(SERVO_PWR_PIN, HIGH);
    return true;
  }
  return false; // no change
}
