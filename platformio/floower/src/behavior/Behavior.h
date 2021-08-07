#pragma once

#include "hardware/Floower.h"

typedef uint8_t state_t;

class Behavior {
    public:
        virtual void init(bool wokeUp = false) = 0;
        virtual void update() = 0;
        virtual bool isIdle() = 0;
};