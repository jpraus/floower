#pragma once

#include "Arduino.h"
#include "Config.h"
#include "hardware/Floower.h"
#include "behavior/Behavior.h"

class Calibration : public Behavior {
    public:
        Calibration(Config *config, Floower *floower, bool autoCalibrateTouch);
        virtual void setup(bool wokeUp = false);
        virtual void loop();
        virtual bool isIdle();

    private:
        void calibrateTouch();
        void calibrateListenSerial();

        Config *config;
        Floower *floower;

        bool autoCalibrateTouch = false;
        uint8_t state = 0;
        uint8_t command = 0;
        String serialValue;

        unsigned long touchSampleMillis;
        uint16_t touchValue;

        unsigned long watchDogsTime = 0;
};