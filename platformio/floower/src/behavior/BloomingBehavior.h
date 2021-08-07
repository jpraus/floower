#pragma once

#include "Arduino.h"
#include "Config.h"
#include "behavior/Behavior.h"
#include "StateMachine.h"
#include "Remote.h"
#include "hardware/Floower.h"

class BloomingBehavior : public StateMachine {
    public:
        BloomingBehavior(Config *config, Floower *floower, Remote *remote);
        virtual void update();

    protected:
        virtual bool onLeafTouch(FloowerTouchEvent event);

    private:
        HsbColor nextRandomColor();

        bool enabled = false;
        state_t state;
        unsigned long colorsUsed = 0;
        bool preventTouchUp = false;
  
};