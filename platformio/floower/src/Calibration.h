#pragma once

#include "Arduino.h"
#include "Config.h"
#include "hardware/Floower.h"
#include "behavior/Behavior.h"

class Calibration : public Behavior {
    public:
        Calibration(Config *config, Floower *floower);
        virtual void init(bool wokeUp = false);
        virtual void update();
        virtual bool isIdle();

    private:
        void calibrateTouch();
        void calibrateListenSerial();

        Config *config;
        Floower *floower;


        uint8_t state = 0;
        uint8_t command = 0;
        String serialValue;

        unsigned long touchSampleMillis;
        uint16_t touchValue;

        unsigned long watchDogsTime = 0;
};