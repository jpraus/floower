#pragma once

#include "behavior/SmartPowerBehavior.h"

class TestBehavior : public SmartPowerBehavior {
    public:
        TestBehavior(Config *config, Floower *floower, RemoteControl *remoteControl);
        virtual void loop();

    protected:
        virtual bool canInitializeBluetooth();
        virtual bool onLeafTouch(FloowerTouchEvent event);

    private:
        unsigned long eventTime;
  
};