#pragma once

#include "behavior/SmartPowerBehavior.h"

class TestBehavior : public SmartPowerBehavior {
    public:
        TestBehavior(Config *config, Floower *floower, BluetoothConnect *bluetoothConnect, WifiConnect *wifiConnect);
        virtual void loop();

    protected:
        virtual bool canInitializeBluetooth();
        virtual bool onLeafTouch(FloowerTouchEvent event);

    private:
        unsigned long eventTime;
  
};