#ifndef AUTOMATON_H
#define AUTOMATON_H

#include "Arduino.h"
#include "config.h"
#include "floower.h"

class Automaton {
  public:
    Automaton(Floower *floower, Config *config);
    void init();
    void update();
    bool canEnterDeepSleep();
    
  private:
    void onLeafTouch(FloowerTouchType touchType);
    void changeState(uint8_t newState);
    RgbColor nextRandomColor();

    Floower *floower;
    Config *config;

    uint8_t state;
    unsigned long colorsUsed = 0;
    bool colorPickerOn = false;
};

#endif
