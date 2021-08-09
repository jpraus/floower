#pragma once

#include "hardware/Floower.h"

typedef uint8_t state_t;

class Behavior {
    public:
        virtual void setup(bool wokeUp = false) = 0;
        virtual void loop() = 0;
        virtual bool isIdle() = 0;
};