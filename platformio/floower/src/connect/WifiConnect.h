#pragma once

#include "Arduino.h"
#include "Config.h"

class WifiConnect {
    public:
        WifiConnect(Config *config);
        void init();

    private:
        void connect();
        void request();

        Config *config;
};
