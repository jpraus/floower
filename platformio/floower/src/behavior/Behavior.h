#pragma once

#include "hardware/Floower.h"

typedef uint8_t state_t;

class Behavior {
    public:
        virtual bool init() = 0; // returns true if behavior should be considered as active
        virtual bool update() = 0; // returns true if behavior should be considered as active
        virtual void suspend() = 0;
        virtual void resume() = 0;
        virtual bool onLeafTouch(FloowerTouchEvent event) = 0; // returns true if touch was handled
};