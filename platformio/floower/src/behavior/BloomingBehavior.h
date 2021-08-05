#pragma once

#include "Arduino.h"
#include "Config.h"
#include "Behavior.h"
#include "Remote.h"
#include "hardware/Floower.h"

class BloomingBehavior : public Behavior {
    public:
        BloomingBehavior(Config *config, Floower *floower, Remote *remote);
        bool init();
        bool update();
        void suspend();
        void resume();
        bool onLeafTouch(FloowerTouchEvent event);

    private:
        HsbColor nextRandomColor();

        Config *config;
        Floower *floower;
        Remote *remote;

        bool enabled = false;
        state_t state;
        unsigned long colorsUsed = 0;
        bool disabledTouchUp = false;
  
};