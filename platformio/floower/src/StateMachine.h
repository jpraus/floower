#pragma once

#include "Arduino.h"
#include "Config.h"
#include "hardware/Floower.h"
#include "Remote.h"
//#include "Behavior.h"

class StateMachine {
    public:
        StateMachine(Config *config, Floower *floower, Remote *remote);
        void init(bool wokeUp = false);
        void update();
        void changeState(uint8_t newState);
        bool isIdle();

    private:
        void enablePeripherals(bool wokeUp = false);
        void disablePeripherals();
        void onLeafTouch(FloowerTouchEvent event);
        void powerWatchDog(bool wokeUp = false);
        void planDeepSleep(long timeoutMs);
        void unplanDeepSleep();
        void enterDeepSleep();

        void indicateStatus(uint8_t status, bool enable);

        Config *config;
        Floower *floower;
        Remote *remote;

        //Behavior *behavior = nullptr;

        bool softPower = false;
        bool lowBattery = false;

        uint8_t state;
        PowerState powerState;

        unsigned long watchDogsTime = 0;
        unsigned long initRemoteTime = 0;
        unsigned long deepSleepTime = 0;

        uint8_t indicatingStatus = 0;
    
};