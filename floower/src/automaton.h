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
    void suspend();

  private:
    void onLeafTouch(FloowerTouchEvent event);
    void onRemoteTookOver();
    void changeState(uint8_t newState);
    HsbColor nextRandomColor();

    Remote *remote;
    Floower *floower;
    Config *config;

    boolean enabled = false;
    uint8_t state;
    unsigned long colorsUsed = 0;
    bool disabledTouchUp = false;
};

#endif
