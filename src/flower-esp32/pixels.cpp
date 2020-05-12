#include "pixels.h"

Pixels::Pixels(int numPixels, byte ledsPin)
    : numPixels(numPixels), ledsPin(ledsPin) {
}

void Pixels::init(byte ledsModel) {
  if (ledsModel == 0) { // WS2812b
    pixelsWS = new NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>(numPixels, ledsPin); // WS2812b
  }
  else {
    pixelsSK = new NeoPixelBus<NeoGrbwFeature, Neo800KbpsMethod>(numPixels, ledsPin); // SK6812
  }
  this->ledsModel = ledsModel;
}

void Pixels::Begin() {
  if (ledsModel == 0) { // WS2812b
    pixelsWS->Begin();
  }
  else {
    pixelsSK->Begin();
  }
}

void Pixels::SetPixelColor(uint16_t indexPixel, RgbColor color) {
  if (ledsModel == 0) { // WS2812b
    pixelsWS->SetPixelColor(indexPixel, color);
  }
  else {
    pixelsSK->SetPixelColor(indexPixel, color);
  }
}

void Pixels::ClearTo(RgbColor color) {
  if (ledsModel == 0) { // WS2812b
    pixelsWS->ClearTo(color);
  }
  else {
    pixelsSK->ClearTo(color);
  }
}

void Pixels::Show() {
  if (ledsModel == 0) { // WS2812b
    pixelsWS->Show();
  }
  else {
    pixelsSK->Show();
  }
}
