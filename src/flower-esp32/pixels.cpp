#include "pixels.h"

Pixels::Pixels(int numPixels, byte ledsPin)
    : numPixels(numPixels), ledsPin(ledsPin) {
}

void Pixels::init(byte ledsModel) {
  if (ledsModel == LEDS_MODEL_WS2812B) {
    pixelsWS = new NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>(numPixels, ledsPin); // WS2812b
  }
  else {
    pixelsSK = new NeoPixelBus<NeoGrbwFeature, Neo800KbpsMethod>(numPixels, ledsPin); // SK6812
  }
  this->ledsModel = ledsModel;
}

void Pixels::Begin() {
  if (ledsModel == LEDS_MODEL_WS2812B) {
    pixelsWS->Begin();
  }
  else {
    pixelsSK->Begin();
  }
}

void Pixels::SetPixelColor(uint16_t indexPixel, RgbColor color) {
  if (ledsModel == LEDS_MODEL_WS2812B) {
    pixelsWS->SetPixelColor(indexPixel, color);
  }
  else {
    pixelsSK->SetPixelColor(indexPixel, color);
  }
}

void Pixels::ClearTo(RgbColor color) {
  if (ledsModel == LEDS_MODEL_WS2812B) {
    pixelsWS->ClearTo(color);
  }
  else {
    pixelsSK->ClearTo(color);
  }
}

void Pixels::Show() {
  if (ledsModel == LEDS_MODEL_WS2812B) {
    pixelsWS->Show();
  }
  else {
    pixelsSK->Show();
  }
}
