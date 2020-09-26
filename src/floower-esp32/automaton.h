#ifndef AUTOMATON_H
#define AUTOMATON_H

#include "Arduino.h"
#include "config.h"
#include "floower.h"
#include "remote.h"

class Automaton {
  public:
    Automaton(Remote *remote, Floower *floower, Config *config);
    void init();
    void update();
    bool canEnterDeepSleep();
    
  private:
    void onLeafTouch(FloowerTouchEvent event);
    void changeState(uint8_t newState);
    RgbColor nextRandomColor();

    Remote *remote;
    Floower *floower;
    Config *config;

    uint8_t state;
    unsigned long colorsUsed = 0;
    bool disabledTouchUp = false;
};

#endif
