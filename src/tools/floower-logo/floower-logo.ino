#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>

const uint16_t pixelPin = 9;
const uint16_t totalPixels = 194;
const uint16_t FPixelsStart = 0; // 21
const uint16_t LPixelsStart = 21; // 16
const uint16_t OPixelsStart = 37; // 57
const uint16_t WPixelsStart = 94; // 40
const uint16_t EPixelsStart = 134; // 29
const uint16_t RPixelsStart = 163; // 31

const float brightness = 0.4;

NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(totalPixels, pixelPin);
NeoPixelAnimator animations(1);

void generateRandomSeed() {
    uint32_t seed;

    // random works best with a seed that can use 31 bits
    // analogRead on a unconnected pin tends toward less than four bits
    seed = analogRead(0);
    delay(1);
    for (int shifts = 3; shifts < 31; shifts += 3) {
        seed ^= analogRead(0) << shifts;
        delay(1);
    }
    Serial.println(seed);
    randomSeed(seed);
}

void LoopAnimUpdate(const AnimationParam& param) {
  uint16_t OPixelsCount = WPixelsStart - OPixelsStart;
  
  for (float i = 0; i < OPixelsCount; i++) {
    float hue = param.progress + (i / OPixelsCount);
    if (hue > 1) {
      hue -= 1;
    }
    strip.SetPixelColor(OPixelsStart + i, HslColor(hue, 1, brightness));
  }
  if (param.state == AnimationState_Completed) {
    // done, time to restart this position tracking animation/timer
    animations.RestartAnimation(param.index);
  }
}

void setup() {
  Serial.begin(115200);
  
  strip.Begin();
  strip.Show();

  generateRandomSeed();
  strip.ClearTo(HslColor(1, 0, brightness));
  animations.StartAnimation(0, 2000, LoopAnimUpdate);
}


void loop() {
    // this is all that is needed to keep it running
    // and avoiding using delay() is always a good thing for
    // any timing related routines
    animations.UpdateAnimations();
    strip.Show();
}
