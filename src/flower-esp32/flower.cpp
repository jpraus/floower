#include "flower.h"

#define NEOPIXEL_PIN 27
#define NEOPIXEL_PWR_PIN 25

#define SERVO_PIN 26
#define SERVO_PWR_PIN 33

#define SERVO_ANGLES_DIFFERENCE 750
#define SERVO_DIR_OPENING 1
#define SERVO_DIR_CLOSING 2

Flower::Flower() : animations(2) {
}

void Flower::init(int closedAngle) {
  // default servo configuration
  servoOpenAngle = closedAngle + SERVO_ANGLES_DIFFERENCE;
  servoClosedAngle = closedAngle;
  servoAngle = closedAngle;
  servoTargetAngle = closedAngle;
  servoDir = SERVO_DIR_CLOSING;
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

  if (newAngle < servoTargetAngle) { // closing
    servoDir = SERVO_DIR_CLOSING;
  }
  else {
    servoDir = SERVO_DIR_OPENING;
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

/*
boolean movePetals() {
  if (servoTarget < configServoClosed) {
    servoTarget = configServoClosed;
  }
  else if (servoTarget > configServoOpen) {
    servoTarget = configServoOpen;
  }

  if (servoTarget == servoPosition) {
    if (servoTarget == configServoOpen && servoDir == SERVO_DIR_OPENING) {
      servoTarget = configServoOpen - 50; // close back a little bit to prevent servo stalling
    }
    else {
      setServoPowerOn(false);
      return true; // finished
    }
  }

  if (servoTarget < servoPosition) {
    servoPosition --;
  }
  else {
    servoPosition ++;
  }
  
  setServoPowerOn(true);
  servo.write(servoPosition);
  return false;
}
*/
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
