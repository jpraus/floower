#pragma once

#include "Arduino.h"
#include "Config.h"
#include "WiFi.h"

#define WIFI_MAX_PAYLOAD_BYTES 255

struct WifiMessageHeader {
    uint16_t type;
    uint16_t id;
    uint16_t length;
} __attribute__((packed));

class WifiConnect {
    public:
        WifiConnect(Config *config);
        void setup();
        void loop();
        void runOTAUpdate();

    private:
        void connect();
        void request();

        bool readMessage();
        int8_t readMessageHeader(WifiMessageHeader& header);

        void sendMessage(WifiMessageHeader& header, const uint8_t* payload, const size_t payloadSize);

        Config *config;
        WiFiClient client;

        unsigned long reconnectTime = 0;
        bool connected = false;
        uint8_t payloadBuffer[WIFI_MAX_PAYLOAD_BYTES + 1]; // extra space for 0 terminating string
};
