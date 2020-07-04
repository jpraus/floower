#include <NeoPixelBus.h>
#include <ESP32Servo.h>

NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(7, 27); // WS2812b
NeoPixelBus<NeoGrbwFeature, Neo800KbpsMethod> strip(7, 27); // SK6812

#define colorSaturation 128
RgbColor red(colorSaturation, 0, 0);
RgbColor green(0, colorSaturation, 0);
RgbColor blue(0, 0, colorSaturation);
RgbColor white(colorSaturation);
RgbColor black(0);

Servo servo;

void setup() {
  Serial.begin(115200);
  while (!Serial); // wait for serial attach

  // power on the LEDs
  pinMode(25, OUTPUT);
  digitalWrite(25, LOW);

  // this resets all the neopixels to an off state
  strip.Begin();
  strip.Show();

  // power on the servo
  pinMode(33, OUTPUT);
  digitalWrite(33, LOW);

  servo.setPeriodHertz(50); // standard 50 hz servo
  servo.attach(26, 700, 1400);
  servo.write(700);
}

void loop() {
    strip.ClearTo(red);
    strip.Show();
    delay(1000);
    
    strip.ClearTo(green);
    strip.Show();
    delay(1000);

    strip.ClearTo(blue);
    strip.Show();
    delay(1000);

    strip.ClearTo(white);
    strip.Show();
    delay(1000);    
}
