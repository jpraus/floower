#pragma once

#include "Arduino.h"
#include "Config.h"
#include "hardware/Floower.h"

struct TouchCalibration {
    uint8_t samples;
    unsigned long nextSampleMillis;
    unsigned int value;
};

class Calibration {
    public:
        Calibration(Floower *floower, Config *config);
        void calibrate();

    private:
        void calibrateTouch();
        void calibrateOverSerial();

        Floower *floower;
        Config *config;

        uint8_t state = 0;
        uint8_t serialCommand = 0;
        String serialValue;
        TouchCalibration touch;
};