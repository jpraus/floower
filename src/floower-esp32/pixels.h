#ifndef PIXELS_H
#define PIXELS_H

#include "Arduino.h"
#include <NeoPixelBus.h>

#define LEDS_MODEL_WS2812B 0
#define LEDS_MODEL_SK6812 1

class Pixels {
  public:
    Pixels(int numPixels, byte ledsPin);
    void init(byte ledsModel);
    void update();

    void Begin();
    void SetPixelColor(uint16_t indexPixel, RgbColor color);
    void ClearTo(RgbColor color);
    void Show();

  private:
    NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> *pixelsWS; // WS2812b
    NeoPixelBus<NeoGrbwFeature, Neo800KbpsMethod> *pixelsSK; // SK6812
  
    byte ledsModel = 0; // 0 - WS2812b, 1 - SK6812

    unsigned int numPixels;
    byte ledsPin;
  
};

#endif
