#pragma once

#include "behavior/SmartPowerBehavior.h"

class BloomingBehavior : public SmartPowerBehavior {
    public:
        BloomingBehavior(Config *config, Floower *floower, BluetoothConnect *bluetoothConnect, WifiConnect *wifiConnect);
        virtual void loop();

    protected:
        virtual bool onLeafTouch(FloowerTouchEvent event);
        virtual bool canInitializeBluetooth();
  
};