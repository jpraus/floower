#ifndef BLOSSOM_H
#define BLOSSOM_H

#include "Arduino.h"
#include "pixels.h"

#define NEOPIXEL_PIN 27 // original 19
#define NEOPIXEL_PWR_PIN 25

class Blossom {
  public:
    Blossom();
    void init();
    void update();

  private:
  
};

#endif
