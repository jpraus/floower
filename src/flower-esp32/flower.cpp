#include "flower.h"

#define NEOPIXEL_PIN 27
#define NEOPIXEL_PWR_PIN 25

#define SERVO_PIN 26
#define SERVO_PWR_PIN 33

Flower::Flower() : animations(2), pixels(7, NEOPIXEL_PIN) {
}

void Flower::init(int closedAngle, int openAngle, byte ledsModel) {
  // default servo configuration
  servoOpenAngle = openAngle;
  servoClosedAngle = closedAngle;
  servoAngle = closedAngle;
  servoOriginAngle = closedAngle;
  servoTargetAngle = closedAngle;
  petalsOpenLevel = 0; // 0-100%

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
}

void Flower::update() {
  animations.UpdateAnimations();

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

  animations.StartAnimation(0, transitionTime, [=](const AnimationParam& param){ servoAnimationUpdate(param); });
}

void Flower::servoAnimationUpdate(const AnimationParam& param) {
  servoAngle = servoOriginAngle + (servoTargetAngle - servoOriginAngle) * param.progress;

  setServoPowerOn(true);
  servo.write(servoAngle);

  if (param.state == AnimationState_Completed) {
    setServoPowerOn(false); // TODO: some timeout to give servo time to finish the operation?
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
