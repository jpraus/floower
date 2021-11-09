#pragma once

#include "behavior/SmartPowerBehavior.h"

class MindfulnessBehavior : public SmartPowerBehavior {
    public:
        MindfulnessBehavior(Config *config, Floower *floower, RemoteControl *remoteControl);
        virtual void loop();

    protected:
        virtual bool onLeafTouch(FloowerTouchEvent event);

    private:
        unsigned long eventTime;
  
};