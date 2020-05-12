#ifndef PIXELS_H
#define PIXELS_H

#include "Arduino.h"
#include <NeoPixelBus.h>

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

    int numPixels;
    byte ledsPin;
  
};

#endif
