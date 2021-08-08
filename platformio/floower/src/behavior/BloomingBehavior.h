#pragma once

#include "Arduino.h"
#include "Config.h"
#include "behavior/Behavior.h"
#include "behavior/SmartPowerBehavior.h"
#include "Remote.h"
#include "hardware/Floower.h"

class BloomingBehavior : public SmartPowerBehavior {
    public:
        BloomingBehavior(Config *config, Floower *floower, Remote *remote);
        virtual void update();

    protected:
        virtual bool onLeafTouch(FloowerTouchEvent event);

    private:
        HsbColor nextRandomColor();

        unsigned long colorsUsed = 0;
        bool preventTouchUp = false;
  
};