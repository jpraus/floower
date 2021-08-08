#pragma once

#include "behavior/SmartPowerBehavior.h"

class BloomingBehavior : public SmartPowerBehavior {
    public:
        BloomingBehavior(Config *config, Floower *floower, BluetoothControl *bluetoothControl);
        virtual void update();

    protected:
        virtual bool onLeafTouch(FloowerTouchEvent event);
        virtual bool canInitializeBluetooth();

    private:
        HsbColor nextRandomColor();
        unsigned long colorsUsed = 0;
  
};