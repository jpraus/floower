#pragma once

#include "Arduino.h"
#include "Config.h"
#include "Behavior.h"
#include "Remote.h"
#include "hardware/Floower.h"

class BloomingBehavior : public Behavior {
    public:
        BloomingBehavior(Config *config, Floower *floower, Remote *remote);
        void init();
        void update();
        void suspend();
        void resume();
        bool isIdle();
        bool isBluetoothPairingAllowed();
        bool onLeafTouch(FloowerTouchEvent event);

    private:
        void changeStateIfIdle(state_t fromState, state_t toState);
        void changeState(state_t newState);
        HsbColor nextRandomColor();

        Config *config;
        Floower *floower;
        Remote *remote;

        bool enabled = false;
        state_t state;
        unsigned long colorsUsed = 0;
        bool preventTouchUp = false;
  
};