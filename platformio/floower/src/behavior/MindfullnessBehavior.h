#pragma once

#include "behavior/SmartPowerBehavior.h"

class MindfullnessBehavior : public SmartPowerBehavior {
    public:
        MindfullnessBehavior(Config *config, Floower *floower, BluetoothControl *bluetoothControl);
        virtual void setup(bool wokeUp = false);
        virtual void loop();

    protected:
        virtual bool onLeafTouch(FloowerTouchEvent event);

    private:
        unsigned long eventTime;
  
};